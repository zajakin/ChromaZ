#include "alignjob.h"
#include "msa.h"
#include "ab1parser.h"
#include <algorithm>
#include <cctype>
#include <array>
#include <thread>

namespace {

std::string cleanSeq(const QString &raw) {
  std::string in = raw.toStdString(), out;
  out.reserve(in.size());
  for (char c : in) if (c != '-' && c != ' ') out.push_back(c);
  return out;
}

QString fromStd(const std::string &s) { return QString::fromStdString(s); }

// Пул потоков на std::thread: fn(i) для i = 0..n-1.
bool parallelFor(int n, int threads, const std::atomic<bool> &cancel, const std::function<void(int)> &fn) {
  // По умолчанию не больше 8 потоков: каждый держит свои матрицы динамического программирования
  // (для длинных контигов — десятки МБ), поэтому на многоядерных машинах память растёт быстро.
  // Явное значение (поле Threads / --threads) не ограничивается.
  int T = threads > 0 ? threads : std::min(8, (int)std::thread::hardware_concurrency());
  if (T <= 0) T = 2;
  T = std::max(1, std::min(T, n));
  std::atomic<int> next{0};
  std::atomic<bool> failed{false};
  auto worker = [&]() {
    for (;;) {
      if (cancel.load() || failed.load()) return;
      int i = next.fetch_add(1);
      if (i >= n) return;
      try { fn(i); } catch (...) { failed = true; return; }
    }
  };
  if (T == 1) { worker(); }
  else {
    std::vector<std::thread> pool;
    pool.reserve(T - 1);
    for (int t = 0; t < T - 1; ++t) pool.emplace_back(worker);
    worker();
    for (auto &th : pool) th.join();
  }
  return !failed.load();
}

} // namespace

// ---------------------------------------------------------------------------
void reinsertDeletedBases(QString &alignedRef, QString &alignedRead, const QString &orientedRaw) {
  const std::string raw = orientedRaw.toStdString();
  if (raw.find(' ') == std::string::npos) return;
  std::string rf = alignedRef.toStdString(), rd = alignedRead.toStdString();
  size_t p = 0;
  for (char ch : raw) {
    if (ch == '-') continue;   // гэпы исходного файла (FASTA) не входили в выравнивание — см. cleanSeq()
    if (ch == ' ') {
      if (p < rd.size() && rd[p] == '-') { rd[p] = ' '; ++p; }      // занимаем соседний гэп
      else { rd.insert(p, 1, ' '); rf.insert(p, 1, '-'); ++p; }     // новая колонка
    } else {
      while (p < rd.size() && rd[p] == '-') ++p;
      ++p;
    }
  }
  alignedRef = fromStd(rf);
  alignedRead = fromStd(rd);
}

// ---------------------------------------------------------------------------
MasterAlignment buildMaster(const QString &ungappedRef, const std::vector<AlignJobTrackOut> &tracks) {
  MasterAlignment m;
  const std::string ref = ungappedRef.toStdString();
  const int N = (int)ref.size();
  const size_t K = tracks.size();
  m.masterReads.resize(K);

  std::vector<std::vector<std::string>> ins(K);
  std::vector<std::string> match(K);
  std::vector<int> maxGaps(N + 1, 0);

  for (size_t k = 0; k < K; ++k) {
    if (!tracks[k].valid) continue;
    ins[k].assign(N + 1, std::string());
    match[k].assign(N, '-');
    const std::string a = tracks[k].alignedRef.toStdString(), b = tracks[k].alignedRead.toStdString();
    size_t len = std::min(a.size(), b.size());
    int refIdx = 0;
    for (size_t p = 0; p < len; ++p) {
      char q = (char)std::toupper((unsigned char)b[p]);
      if (a[p] != '-') { if (refIdx < N) match[k][refIdx++] = q; }
      else ins[k][std::min(refIdx, N)].push_back(q);
    }
    for (int s = 0; s <= N; ++s) maxGaps[s] = std::max(maxGaps[s], (int)ins[k][s].size());
  }

  std::string masterRef(maxGaps[0], '-');
  for (int i = 0; i < N; ++i) { masterRef.push_back(ref[i]); masterRef.append(maxGaps[i + 1], '-'); }
  m.masterRef = fromStd(masterRef);

  for (size_t k = 0; k < K; ++k) {
    if (!tracks[k].valid) continue;
    std::string r;
    r.reserve(masterRef.size());
    const std::string &i0 = ins[k][0];
    r += i0; r.append(maxGaps[0] - (int)i0.size(), '-');
    for (int i = 0; i < N; ++i) {
      r.push_back(match[k][i]);
      const std::string &s = ins[k][i + 1];
      r += s; r.append(maxGaps[i + 1] - (int)s.size(), '-');
    }
    m.masterReads[k] = fromStd(r);
  }
  return m;
}

// ---------------------------------------------------------------------------
QString consensusFromMaster(const MasterAlignment &master, const std::vector<bool> &voter) {
  const std::string ref = master.masterRef.toStdString();
  const size_t L = ref.size();
  std::vector<std::array<int, 4>> cnt(L, std::array<int, 4>{{0, 0, 0, 0}});
  std::vector<int> gaps(L, 0);
  for (size_t k = 0; k < master.masterReads.size(); ++k) {
    if (k >= voter.size() || !voter[k] || master.masterReads[k].isEmpty()) continue;
    const std::string r = master.masterReads[k].toStdString();
    size_t first = r.find_first_not_of('-'), last = r.find_last_not_of('-');
    if (first == std::string::npos) continue;
    for (size_t c = first; c <= last && c < L; ++c) {
      switch (std::toupper((unsigned char)r[c])) {
      case 'A': cnt[c][0]++; break;
      case 'C': cnt[c][1]++; break;
      case 'G': cnt[c][2]++; break;
      case 'T': cnt[c][3]++; break;
      case '-': gaps[c]++; break;
      default: break; // N, IUPAC, ' ' — не голосуют
      }
    }
  }
  std::string out;
  for (size_t c = 0; c < L; ++c) {
    int best = 0, total = 0;
    for (int x = 0; x < 4; ++x) { best = std::max(best, cnt[c][x]); total += cnt[c][x]; }
    if (total == 0 || gaps[c] > total) continue;     // нет данных или большинство за гэп
    const char letters[4] = {'A', 'C', 'G', 'T'};
    char pick = 0;
    char refBase = (char)std::toupper((unsigned char)ref[c]);
    for (int x = 0; x < 4; ++x) if (cnt[c][x] == best && letters[x] == refBase) pick = refBase; // при ничьей — база референса
    if (!pick) for (int x = 0; x < 4; ++x) if (cnt[c][x] == best) { pick = letters[x]; break; }
    out.push_back(pick);
  }
  return fromStd(out);
}

// ---------------------------------------------------------------------------
AlignJobOutput runAlignJob(const AlignJobInput &in, const std::atomic<bool> &cancel, const AlignProgressFn &progress) {
  AlignJobOutput out;
  const int n = (int)in.tracks.size();
  out.tracks.assign(n, AlignJobTrackOut());

  std::vector<QString> clean(n);
  int nonEmpty = 0;
  for (int i = 0; i < n; ++i) { clean[i] = fromStd(cleanSeq(in.tracks[i].rawSeq)); if (!clean[i].isEmpty()) ++nonEmpty; }
  if (nonEmpty == 0) return out;

  const bool autoRef = in.reference.isEmpty();
  const bool msaMode = (in.algo == AlignmentAlgorithm::InternalClustal);
  const int rounds = autoRef ? std::max(0, std::min(in.refineIterations, 10)) : 0;

  std::atomic<int> done{0};
  int total = nonEmpty * (1 + rounds) + ((autoRef && in.autoRC) ? nonEmpty : 0) + (msaMode ? nonEmpty * (1 + rounds) : 0);
  auto tick = [&](const QString &stage) { int d = ++done; if (progress) progress(std::min(d, total), total, stage); };
  auto stageMsg = [&](const char *s) { if (progress) progress(done.load(), total, QString::fromLatin1(s)); };

  auto finishCancelled = [&]() { out.cancelled = true; return out; };

  // ---- референс
  QString ref = in.reference;
  if (autoRef) {
    int best = -1, bestLen = 0;
    for (int i = 0; i < n; ++i) if (clean[i].length() > bestLen) { bestLen = clean[i].length(); best = i; }
    QString seed = clean[best];
    if (in.autoRC) {
      stageMsg("Choosing seed orientation");
      std::atomic<int> fwdVotes{0}, revVotes{0};
      bool ok = parallelFor(n, in.threads, cancel, [&](int i) {
        if (i == best || clean[i].isEmpty()) return;
        AlignmentResult f = AlignmentEngine::alignSemiGlobal(seed, clean[i], in.algo, AlignmentEngine::RcMode::ForceForward);
        AlignmentResult r = AlignmentEngine::alignSemiGlobal(seed, Ab1Parser::reverseComplement(clean[i]), in.algo, AlignmentEngine::RcMode::ForceForward);
        if (r.score > f.score) ++revVotes; else ++fwdVotes;
        tick("Choosing seed orientation");
      });
      if (!ok || cancel.load()) return finishCancelled();
      if (revVotes > fwdVotes) seed = Ab1Parser::reverseComplement(seed);
    } else if (in.tracks[best].fixedRC) {
      seed = Ab1Parser::reverseComplement(seed);
    }
    ref = seed;
  }
  if (ref.isEmpty()) return out;

  // ---- выравнивание всех ридов к референсу
  auto alignAll = [&](const QString &reference, const QString &stage) -> bool {
    std::vector<QString> oriented(n);
    bool ok = parallelFor(n, in.threads, cancel, [&](int i) {
      AlignJobTrackOut &o = out.tracks[i];
      o = AlignJobTrackOut();
      if (clean[i].isEmpty()) return;
      AlignmentEngine::RcMode mode = AlignmentEngine::RcMode::Auto;
      if (!in.autoRC || in.tracks[i].fixedOrientation)
        mode = in.tracks[i].fixedRC ? AlignmentEngine::RcMode::ForceReverse : AlignmentEngine::RcMode::ForceForward;
      AlignmentResult r = AlignmentEngine::alignSemiGlobal(reference, clean[i], in.algo, mode);
      o.valid = true; o.isRC = r.isReverseComplement; o.identity = r.identity; o.score = r.score;
      o.alignedRef = r.alignedRef; o.alignedRead = r.alignedRead;
      oriented[i] = r.isReverseComplement ? Ab1Parser::reverseComplement(clean[i]) : clean[i];
      tick(stage);
    });
    if (!ok || cancel.load()) return false;

    if (msaMode) {
      // Настоящее прогрессивное MSA: референс + все риды в найденной ориентации.
      std::vector<int> idx;
      std::vector<std::string> rows{reference.toStdString()};
      for (int i = 0; i < n; ++i) if (out.tracks[i].valid) { idx.push_back(i); rows.push_back(oriented[i].toStdString()); }
      stageMsg("Progressive multiple alignment");
      auto cancelFn = [&]() { return cancel.load(); };
      std::vector<std::string> msa = Msa::progressiveAlign(rows, cancelFn);
      if (msa.empty() || cancel.load()) return false;
      const std::string &r0 = msa[0];
      for (size_t k = 0; k < idx.size(); ++k) {
        const std::string &rk = msa[k + 1];
        std::string a, b;
        for (size_t c = 0; c < r0.size(); ++c) {
          if (r0[c] == '-' && rk[c] == '-') continue;
          a.push_back(r0[c]); b.push_back(rk[c]);
        }
        AlignJobTrackOut &o = out.tracks[idx[k]];
        o.alignedRef = fromStd(a); o.alignedRead = fromStd(b);
        o.identity = AlignmentEngine::overlapIdentity(o.alignedRef, o.alignedRead);
        tick(stage);
      }
    }

    // Вернуть удалённые основания (' '), чтобы не терять данные хроматограммы в середине рида.
    for (int i = 0; i < n; ++i) {
      if (!out.tracks[i].valid) continue;
      QString orientedRaw = out.tracks[i].isRC ? Ab1Parser::reverseComplement(in.tracks[i].rawSeq) : in.tracks[i].rawSeq;
      reinsertDeletedBases(out.tracks[i].alignedRef, out.tracks[i].alignedRead, orientedRaw);
    }
    return true;
  };

  if (!alignAll(ref, "Aligning reads")) return finishCancelled();

  // ---- итеративное уточнение: консенсус становится новым референсом
  for (int round = 0; round < rounds; ++round) {
    MasterAlignment m = buildMaster(ref, out.tracks);
    std::vector<bool> voters(n, false);
    for (int i = 0; i < n; ++i) voters[i] = out.tracks[i].valid && out.tracks[i].identity >= in.minIdentity;
    QString cons = consensusFromMaster(m, voters);
    if (cons.isEmpty() || cons == ref) break;
    ref = cons;
    if (!alignAll(ref, QString("Refining consensus (round %1)").arg(round + 1))) return finishCancelled();
    out.refineRounds = round + 1;
  }

  out.reference = ref;
  out.autoReference = autoRef;
  return out;
}
