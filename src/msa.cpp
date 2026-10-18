#include "msa.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <limits>

namespace Msa {
namespace {

int symIndex(char c) {
  switch (std::toupper(static_cast<unsigned char>(c))) {
  case 'A': return 0;
  case 'C': return 1;
  case 'G': return 2;
  case 'T': case 'U': return 3;
  default: return -1;
  }
}

// Отсортированный набор уникальных k-мер (только ACGT).
std::vector<uint32_t> kmerSet(const std::string &s, int k) {
  std::vector<uint32_t> out;
  if ((int)s.size() < k) return out;
  const uint32_t mask = (k >= 16) ? 0xFFFFFFFFu : ((1u << (2 * k)) - 1u);
  uint32_t code = 0;
  int valid = 0;
  for (char ch : s) {
    int x = symIndex(ch);
    if (x < 0) { valid = 0; code = 0; continue; }
    code = ((code << 2) | (uint32_t)x) & mask;
    if (++valid >= k) out.push_back(code);
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

double kmerDistance(const std::vector<uint32_t> &a, const std::vector<uint32_t> &b) {
  size_t m = std::min(a.size(), b.size());
  if (m == 0) return 1.0;
  size_t i = 0, j = 0, shared = 0;
  while (i < a.size() && j < b.size()) {
    if (a[i] == b[j]) { ++shared; ++i; ++j; }
    else if (a[i] < b[j]) ++i;
    else ++j;
  }
  double d = 1.0 - (double)shared / (double)m;
  return std::min(1.0, std::max(0.0, d));
}

struct Profile {
  std::vector<std::string> rows; // выровненные строки
  std::vector<int> ids;          // индексы исходных последовательностей
  int length() const { return rows.empty() ? 0 : (int)rows[0].size(); }
};

struct Column {
  std::array<float, 4> f{{0, 0, 0, 0}}; // доли A,C,G,T среди всех строк профиля
  float nongap = 0;                     // доля строк с остатком (не гэп)
};

std::vector<Column> columnsOf(const Profile &p) {
  int L = p.length();
  std::vector<Column> cols(L);
  float n = (float)p.rows.size();
  if (n == 0) return cols;
  for (const auto &r : p.rows) {
    for (int c = 0; c < L; ++c) {
      if (r[c] == '-') continue;
      cols[c].nongap += 1.0f;
      int x = symIndex(r[c]);
      if (x >= 0) cols[c].f[x] += 1.0f;
    }
  }
  for (auto &c : cols) { for (auto &v : c.f) v /= n; c.nongap /= n; }
  return cols;
}

// Склейка без перекрытия — запасной вариант для огромных профилей.
Profile concatenate(const Profile &a, const Profile &b) {
  Profile out;
  int la = a.length(), lb = b.length();
  for (size_t k = 0; k < a.rows.size(); ++k) { out.rows.push_back(a.rows[k] + std::string(lb, '-')); out.ids.push_back(a.ids[k]); }
  for (size_t k = 0; k < b.rows.size(); ++k) { out.rows.push_back(std::string(la, '-') + b.rows[k]); out.ids.push_back(b.ids[k]); }
  return out;
}

bool alignProfiles(const Profile &a, const Profile &b, const std::function<bool()> &cancelled, Profile &out) {
  const int LA = a.length(), LB = b.length();
  if ((double)(LA + 1) * (double)(LB + 1) > 150e6) { out = concatenate(a, b); return true; }

  const float MATCH = 5.0f, MISMATCH = -4.0f, GAP = 6.0f;
  const float S[4][4] = {{MATCH, MISMATCH, MISMATCH, MISMATCH}, {MISMATCH, MATCH, MISMATCH, MISMATCH},
                         {MISMATCH, MISMATCH, MATCH, MISMATCH}, {MISMATCH, MISMATCH, MISMATCH, MATCH}};
  std::vector<Column> ca = columnsOf(a), cb = columnsOf(b);

  // g[i][y] = sum_x fa[i][x] * S[x][y]
  std::vector<std::array<float, 4>> g(LA);
  for (int i = 0; i < LA; ++i)
    for (int y = 0; y < 4; ++y) {
      float s = 0;
      for (int x = 0; x < 4; ++x) s += ca[i].f[x] * S[x][y];
      g[i][y] = s;
    }
  // Стоимость пропуска колонки пропорциональна доле остатков в ней.
  auto gapCostA = [&](int i) { return GAP * (0.25f + 0.75f * ca[i].nongap); };
  auto gapCostB = [&](int j) { return GAP * (0.25f + 0.75f * cb[j].nongap); };

  enum : uint8_t { DIAG = 0, UP = 1, LEFT = 2, STOP = 3 };
  std::vector<uint8_t> tb((size_t)(LA + 1) * (LB + 1), STOP);
  std::vector<float> prev(LB + 1, 0.0f), cur(LB + 1, 0.0f);
  std::vector<float> lastCol(LA + 1, 0.0f); // H[i][LB] — для выбора конца в последнем столбце
  auto T = [&](int i, int j) -> uint8_t & { return tb[(size_t)i * (LB + 1) + j]; };

  // Первая строка/столбец: бесплатные свисающие концы.
  for (int i = 1; i <= LA; ++i) {
    if (cancelled && (i & 31) == 0 && cancelled()) return false;
    cur[0] = 0.0f; T(i, 0) = STOP;
    for (int j = 1; j <= LB; ++j) {
      const Column &cj = cb[j - 1];
      float sc = g[i - 1][0] * cj.f[0] + g[i - 1][1] * cj.f[1] + g[i - 1][2] * cj.f[2] + g[i - 1][3] * cj.f[3];
      float diag = prev[j - 1] + sc;
      float up = prev[j] - gapCostA(i - 1);
      float left = cur[j - 1] - gapCostB(j - 1);
      float best = diag; uint8_t dir = DIAG;
      if (up > best) { best = up; dir = UP; }
      if (left > best) { best = left; dir = LEFT; }
      cur[j] = best; T(i, j) = dir;
    }
    lastCol[i] = cur[LB];
    std::swap(prev, cur);
  }
  // Конец выравнивания: лучшая ячейка последней строки или последнего столбца.
  int bi = LA, bj = LB;
  float bestScore = -std::numeric_limits<float>::infinity();
  for (int j = 0; j <= LB; ++j) if (prev[j] > bestScore) { bestScore = prev[j]; bi = LA; bj = j; }
  for (int i = 0; i <= LA; ++i) if (lastCol[i] > bestScore) { bestScore = lastCol[i]; bi = i; bj = LB; }

  const size_t na = a.rows.size(), nb = b.rows.size();
  std::vector<std::string> ra(na), rb(nb); // строятся с конца
  auto putA = [&](int col) { for (size_t k = 0; k < na; ++k) ra[k].push_back(col >= 0 ? a.rows[k][col] : '-'); };
  auto putB = [&](int col) { for (size_t k = 0; k < nb; ++k) rb[k].push_back(col >= 0 ? b.rows[k][col] : '-'); };

  int i = LA, j = LB;
  for (; j > bj; --j) { putA(-1); putB(j - 1); }   // хвост B после конца выравнивания
  for (; i > bi; --i) { putA(i - 1); putB(-1); }   // хвост A
  while (i > 0 || j > 0) {
    uint8_t d = (i > 0 && j > 0) ? T(i, j) : STOP;
    if (d == STOP) {
      if (i > 0) { putA(i - 1); putB(-1); --i; }
      else       { putA(-1); putB(j - 1); --j; }
    } else if (d == DIAG) { putA(i - 1); putB(j - 1); --i; --j; }
    else if (d == UP)     { putA(i - 1); putB(-1); --i; }
    else                  { putA(-1); putB(j - 1); --j; }
  }
  for (auto &r : ra) std::reverse(r.begin(), r.end());
  for (auto &r : rb) std::reverse(r.begin(), r.end());

  out.rows.clear(); out.ids.clear();
  for (size_t k = 0; k < na; ++k) { out.rows.push_back(std::move(ra[k])); out.ids.push_back(a.ids[k]); }
  for (size_t k = 0; k < nb; ++k) { out.rows.push_back(std::move(rb[k])); out.ids.push_back(b.ids[k]); }
  return true;
}

} // namespace

std::vector<std::string> progressiveAlign(const std::vector<std::string> &seqs,
                                          const std::function<bool()> &cancelled,
                                          const std::function<void(int, int)> &progress) {
  const int N = (int)seqs.size();
  if (N == 0) return {};
  if (N == 1) return {seqs[0]};

  // 1. Расстояния
  std::vector<std::vector<uint32_t>> ks(N);
  for (int i = 0; i < N; ++i) ks[i] = kmerSet(seqs[i], 6);
  std::vector<std::vector<double>> dist(N, std::vector<double>(N, 0.0));
  for (int i = 0; i < N; ++i)
    for (int j = i + 1; j < N; ++j) dist[i][j] = dist[j][i] = kmerDistance(ks[i], ks[j]);
  if (cancelled && cancelled()) return {};

  // 2. UPGMA: узлы 0..N-1 — листья, далее — внутренние в порядке слияния.
  struct Merge { int a, b; };
  std::vector<Merge> merges;
  std::vector<int> nodeOf(N);              // номер узла для каждого активного кластера
  std::vector<int> size(N, 1);
  std::vector<bool> active(N, true);
  for (int i = 0; i < N; ++i) nodeOf[i] = i;
  for (int step = 0; step < N - 1; ++step) {
    double best = std::numeric_limits<double>::infinity();
    int bi = -1, bj = -1;
    for (int i = 0; i < N; ++i) {
      if (!active[i]) continue;
      for (int j = i + 1; j < N; ++j)
        if (active[j] && dist[i][j] < best) { best = dist[i][j]; bi = i; bj = j; }
    }
    merges.push_back({nodeOf[bi], nodeOf[bj]});
    for (int k = 0; k < N; ++k) {
      if (!active[k] || k == bi || k == bj) continue;
      double d = (dist[bi][k] * size[bi] + dist[bj][k] * size[bj]) / (size[bi] + size[bj]);
      dist[bi][k] = dist[k][bi] = d;
    }
    size[bi] += size[bj];
    active[bj] = false;
    nodeOf[bi] = N + step;
  }

  // 3. Прогрессивное выравнивание по дереву
  std::vector<Profile> prof(2 * N - 1);
  std::vector<bool> have(2 * N - 1, false);
  for (int i = 0; i < N; ++i) { prof[i].rows = {seqs[i]}; prof[i].ids = {i}; have[i] = true; }
  int done = 0;
  for (size_t m = 0; m < merges.size(); ++m) {
    if (cancelled && cancelled()) return {};
    int na = merges[m].a, nb = merges[m].b, nn = N + (int)m;
    if (!alignProfiles(prof[na], prof[nb], cancelled, prof[nn])) return {};
    prof[na] = Profile(); prof[nb] = Profile();
    have[nn] = true;
    if (progress) progress(++done, (int)merges.size());
  }

  const Profile &root = prof[2 * N - 2];
  std::vector<std::string> result(N);
  for (size_t k = 0; k < root.ids.size(); ++k) result[root.ids[k]] = root.rows[k];
  return result;
}

} // namespace Msa
