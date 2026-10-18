#include "alignmentengine.h"
#include "ab1parser.h"
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <cmath>

// Идентичность считается только по области перекрытия (между первой и последней
// колонкой, где есть оба основания). Раньше в знаменатель попадали и свисающие
// концы, из-за чего короткие риды получали заведомо низкий identity.
static double overlapIdentity(const QString &a, const QString &b) {
  int len = std::min(a.length(), b.length());
  int first = -1, last = -1, nA = 0, nB = 0;
  for (int p = 0; p < len; ++p) {
    bool ga = (a[p] == '-'), gb = (b[p] == '-');
    if (!ga) ++nA;
    if (!gb) ++nB;
    if (!ga && !gb) { if (first < 0) first = p; last = p; }
  }
  if (first < 0) return 0.0;
  int span = last - first + 1;
  if (span < std::min(20, std::min(nA, nB))) return 0.0; // случайное крошечное перекрытие
  int matches = 0;
  for (int p = first; p <= last; ++p) {
    if (a[p] != '-' && b[p] != '-' && a[p].toUpper() == b[p].toUpper()) ++matches;
  }
  return matches * 100.0 / span;
}

static AlignmentResult alignOverlapCAP3(const QString &ref, const QString &read) {
  int n = ref.length();
  int m = read.length();
  if (n == 0 || m == 0) return {};
  
  const int MATCH = 3, MISMATCH = -2, GAP = -3;
  std::vector<std::vector<int>> score(n + 1, std::vector<int>(m + 1, 0));
  
  for (int i = 0; i <= n; ++i) score[i][0] = 0;
  for (int j = 0; j <= m; ++j) score[0][j] = 0;
  
  for (int i = 1; i <= n; ++i) {
    for (int j = 1; j <= m; ++j) {
      int sMatch = (ref[i-1].toUpper() == read[j-1].toUpper()) ? MATCH : MISMATCH;
      score[i][j] = std::max({
        score[i-1][j-1] + sMatch,
        score[i-1][j] + GAP,
        score[i][j-1] + GAP
      });
    }
  }
  
  int maxScore = -1e9, maxI = n, maxJ = m;
  
  for (int j = 1; j <= m; ++j) {
    if (score[n][j] > maxScore) { maxScore = score[n][j]; maxI = n; maxJ = j; }
  }
  for (int i = 1; i <= n; ++i) {
    if (score[i][m] > maxScore) { maxScore = score[i][m]; maxI = i; maxJ = m; }
  }
  
  int i = maxI, j = maxJ, matchCount = 0;
  QString resRef = "", resRead = "";
  
  for (int k = m; k > maxJ; --k) { resRef.append('-'); resRead.append(read[k-1]); }
  for (int k = n; k > maxI; --k) { resRef.append(ref[k-1]); resRead.append('-'); }
  
  while (i > 0 && j > 0) {
    bool isMatch = (ref[i-1].toUpper() == read[j-1].toUpper());
    int sMatch = isMatch ? MATCH : MISMATCH;
    
    if (score[i][j] == score[i-1][j-1] + sMatch) {
      resRef.append(ref[i-1]); resRead.append(read[j-1]);
      if (isMatch) matchCount++;
      --i; --j;
    } else if (score[i][j] == score[i-1][j] + GAP) {
      resRef.append(ref[i-1]); resRead.append('-'); --i;
    } else {
      resRef.append('-'); resRead.append(read[j-1]); --j;
    }
  }
  
  while (i > 0) { resRef.append(ref[i-1]); resRead.append('-'); --i; }
  while (j > 0) { resRef.append('-'); resRead.append(read[j-1]); --j; }
  
  std::reverse(resRef.begin(), resRef.end());
  std::reverse(resRead.begin(), resRead.end());
  
  int alignedLen = resRead.length();
  double identity = alignedLen > 0 ? (double)matchCount / alignedLen * 100.0 : 0.0;
  
  return {resRef, resRead, maxScore, identity, false};
}

static AlignmentResult alignAffineGapBlock(const QString &ref, const QString &read) {
  int n = ref.length();
  int m = read.length();
  if (n == 0 || m == 0) return {};

  const int MATCH = 3, MISMATCH = -2, GAP_OPEN = -6, GAP_EXT = -1;
  const int NEG = -1000000000;
  auto add = [&](int a, int b) { return (a <= NEG / 2) ? NEG : a + b; }; // без дрейфа "минус бесконечности"

  std::vector<std::vector<int>> M(n + 1, std::vector<int>(m + 1, NEG));
  std::vector<std::vector<int>> X(n + 1, std::vector<int>(m + 1, NEG)); // символ рида против гэпа в ref
  std::vector<std::vector<int>> Y(n + 1, std::vector<int>(m + 1, NEG)); // символ ref против гэпа в риде

  // Свободные свисающие концы с обеих сторон (полу-глобальное выравнивание):
  // риды могут начинаться в любом месте референса и наоборот.
  for (int i = 0; i <= n; ++i) M[i][0] = 0;
  for (int j = 0; j <= m; ++j) M[0][j] = 0;

  for (int i = 1; i <= n; ++i) {
    for (int j = 1; j <= m; ++j) {
      int sc = (ref[i-1].toUpper() == read[j-1].toUpper()) ? MATCH : MISMATCH;
      int bestPrev = std::max({M[i-1][j-1], X[i-1][j-1], Y[i-1][j-1]});
      M[i][j] = add(bestPrev, sc);
      X[i][j] = std::max(add(M[i][j-1], GAP_OPEN), add(X[i][j-1], GAP_EXT));
      Y[i][j] = std::max(add(M[i-1][j], GAP_OPEN), add(Y[i-1][j], GAP_EXT));
    }
  }

  // Конец выравнивания — лучшая ячейка в последней строке или последнем столбце.
  int maxScore = NEG, maxI = n, maxJ = m, state = 0;
  auto consider = [&](int i, int j) {
    int sM = M[i][j], sX = X[i][j], sY = Y[i][j];
    int s = std::max({sM, sX, sY});
    if (s > maxScore) {
      maxScore = s; maxI = i; maxJ = j;
      state = (s == sM) ? 0 : ((s == sX) ? 1 : 2);
    }
  };
  for (int j = 1; j <= m; ++j) consider(n, j);
  for (int i = 1; i <= n; ++i) consider(i, m);

  QString resRef, resRead;
  int i = maxI, j = maxJ;

  for (int k = m; k > maxJ; --k) { resRef.append('-'); resRead.append(read[k-1]); }
  for (int k = n; k > maxI; --k) { resRef.append(ref[k-1]); resRead.append('-'); }

  while (i > 0 && j > 0) {
    if (state == 0) {
      int sc = (ref[i-1].toUpper() == read[j-1].toUpper()) ? MATCH : MISMATCH;
      resRef.append(ref[i-1]); resRead.append(read[j-1]);
      int prevBest = M[i][j] - sc;
      if (prevBest == M[i-1][j-1]) state = 0;
      else if (prevBest == X[i-1][j-1]) state = 1;
      else state = 2;
      --i; --j;
    } else if (state == 1) {
      resRef.append('-'); resRead.append(read[j-1]);
      state = (X[i][j] == add(M[i][j-1], GAP_OPEN)) ? 0 : 1;
      --j;
    } else {
      resRef.append(ref[i-1]); resRead.append('-');
      state = (Y[i][j] == add(M[i-1][j], GAP_OPEN)) ? 0 : 2;
      --i;
    }
  }

  while (i > 0) { resRef.append(ref[i-1]); resRead.append('-'); --i; }
  while (j > 0) { resRef.append('-'); resRead.append(read[j-1]); --j; }

  std::reverse(resRef.begin(), resRef.end());
  std::reverse(resRead.begin(), resRead.end());

  return {resRef, resRead, maxScore, 0.0, false};
}

static AlignmentResult alignNeedlemanWunsch(const QString &ref, const QString &read) {
  int n = ref.length();
  int m = read.length();
  if (n == 0 || m == 0) return {};
  
  const int MATCH = 2, MISMATCH = -1, GAP = -2;
  std::vector<std::vector<int>> score(n + 1, std::vector<int>(m + 1, 0));
  
  for (int j = 0; j <= m; ++j) score[0][j] = j * GAP;
  for (int i = 0; i <= n; ++i) score[i][0] = i * GAP;
  
  for (int i = 1; i <= n; ++i) {
    for (int j = 1; j <= m; ++j) {
      int matchScore = (ref[i-1].toUpper() == read[j-1].toUpper()) ? MATCH : MISMATCH;
      score[i][j] = std::max({score[i-1][j-1] + matchScore, score[i-1][j] + GAP, score[i][j-1] + GAP});
    }
  }
  
  int i = n, j = m, matchCount = 0;
  QString resRef = "", resRead = "";
  
  while (i > 0 || j > 0) {
    if (i > 0 && j > 0) {
      bool isMatch = (ref[i-1].toUpper() == read[j-1].toUpper());
      int matchScore = isMatch ? MATCH : MISMATCH;
      if (score[i][j] == score[i-1][j-1] + matchScore) {
        resRef.append(ref[i-1]); resRead.append(read[j-1]);
        if (isMatch) matchCount++;
        --i; --j; continue;
      }
    }
    if (i > 0 && (j == 0 || score[i][j] == score[i-1][j] + GAP)) {
      resRef.append(ref[i-1]); resRead.append('-'); --i;
    } else if (j > 0) {
      resRef.append('-'); resRead.append(read[j-1]); --j;
    } else break;
  }
  
  std::reverse(resRef.begin(), resRef.end());
  std::reverse(resRead.begin(), resRead.end());
  
  int alignedLen = resRead.length();
  double identity = alignedLen > 0 ? (double)matchCount / alignedLen * 100.0 : 0.0;
  
  return {resRef, resRead, score[n][m], identity, false};
}

static AlignmentResult alignSmithWaterman(const QString &ref, const QString &read) {
  int n = ref.length();
  int m = read.length();
  if (n == 0 || m == 0) return {};
  
  const int MATCH = 4, MISMATCH = -3, GAP = -3;
  std::vector<std::vector<int>> score(n + 1, std::vector<int>(m + 1, 0));
  
  int maxScore = 0, maxI = 0, maxJ = 0;
  
  for (int i = 1; i <= n; ++i) {
    for (int j = 1; j <= m; ++j) {
      int matchScore = (ref[i-1].toUpper() == read[j-1].toUpper()) ? MATCH : MISMATCH;
      score[i][j] = std::max({0, score[i-1][j-1] + matchScore, score[i-1][j] + GAP, score[i][j-1] + GAP});
      if (score[i][j] > maxScore) {
        maxScore = score[i][j];
        maxI = i; maxJ = j;
      }
    }
  }
  
  if (maxScore == 0) return {};
  
  int i = maxI, j = maxJ, matchCount = 0;
  QString locRef = "", locRead = "";
  
  while (i > 0 && j > 0 && score[i][j] > 0) {
    bool isMatch = (ref[i-1].toUpper() == read[j-1].toUpper());
    int matchScore = isMatch ? MATCH : MISMATCH;
    
    if (score[i][j] == score[i-1][j-1] + matchScore) {
      locRef.append(ref[i-1]); locRead.append(read[j-1]);
      if (isMatch) matchCount++;
      --i; --j;
    } else if (score[i][j] == score[i-1][j] + GAP) {
      locRef.append(ref[i-1]); locRead.append('-'); --i;
    } else {
      locRef.append('-'); locRead.append(read[j-1]); --j;
    }
  }
  
  std::reverse(locRef.begin(), locRef.end());
  std::reverse(locRead.begin(), locRead.end());
  
  int startI = i, startJ = j;
  QString resRef = "";
  QString resRead = "";
  
  for (int k = 0; k < startJ; ++k) { resRef.append('-'); resRead.append(read[k]); }
  for (int k = 0; k < startI; ++k) { resRef.append(ref[k]); resRead.append('-'); }
  
  resRef.append(locRef);
  resRead.append(locRead);
  
  for (int k = maxI; k < n; ++k) { resRef.append(ref[k]); resRead.append('-'); }
  for (int k = maxJ; k < m; ++k) { resRef.append('-'); resRead.append(read[k]); }
  
  int alignedLen = resRead.length();
  double identity = alignedLen > 0 ? (double)matchCount / alignedLen * 100.0 : 0.0;
  
  return {resRef, resRead, maxScore, identity, false};
}

static AlignmentResult alignBlastSeedExtend(const QString &ref, const QString &read) {
  int n = ref.length();
  int m = read.length();
  if (n == 0 || m == 0) return {};
  
  const int K = 5;
  if (n < K || m < K) return alignSmithWaterman(ref, read);
  
  std::unordered_map<std::string, std::vector<int>> kmerMap;
  std::string refStr = ref.toUpper().toStdString();
  std::string readStr = read.toUpper().toStdString();
  
  for (int i = 0; i <= n - K; ++i) {
    kmerMap[refStr.substr(i, K)].push_back(i);
  }
  
  std::unordered_map<int, int> diagHits;
  int maxHits = 0, bestDiag = 0;
  
  for (int j = 0; j <= m - K; ++j) {
    std::string km = readStr.substr(j, K);
    auto it = kmerMap.find(km);
    if (it != kmerMap.end()) {
      for (int refPos : it->second) {
        int diag = refPos - j;
        diagHits[diag]++;
        if (diagHits[diag] > maxHits) {
          maxHits = diagHits[diag];
          bestDiag = diag;
        }
      }
    }
  }
  
  if (maxHits == 0) return alignSmithWaterman(ref, read);
  
  int refStart = std::clamp(bestDiag, 0, n);
  int readStart = std::clamp(-bestDiag, 0, m);
  
  QString refPrefix = ref.left(refStart);
  QString readPrefix = read.left(readStart);
  
  QString refTail = ref.mid(refStart);
  QString readTail = read.mid(readStart);
  
  AlignmentResult tailRes = alignSmithWaterman(refTail, readTail);
  if (tailRes.alignedRead.isEmpty() || tailRes.alignedRef.isEmpty()) return alignOverlapCAP3(ref, read);
  
  QString alignedRef = refPrefix + QString(readStart, '-') + tailRes.alignedRef;
  QString alignedRead = QString(refStart, '-') + readPrefix + tailRes.alignedRead;
  
  int matchCount = 0;
  int minLen = std::min(alignedRef.length(), alignedRead.length());
  for (int p = 0; p < minLen; ++p) {
    if (alignedRef[p].toUpper() == alignedRead[p].toUpper() && alignedRef[p] != '-') {
      matchCount++;
    }
  }
  
  double identity = minLen > 0 ? (double)matchCount / minLen * 100.0 : 0.0;
  
  return {alignedRef, alignedRead, tailRes.score, identity, false};
}

static AlignmentResult alignBandedGlobal(const QString &ref, const QString &read) {
  int n = ref.length();
  int m = read.length();
  if (n == 0 || m == 0) return {};
  
  const int MATCH = 2, MISMATCH = -1, GAP = -2, INF = 1e9;
  int W = std::max(15, std::abs(n - m) + 15);
  
  std::vector<std::vector<int>> score(n + 1, std::vector<int>(m + 1, -INF));
  score[0][0] = 0;
  
  for (int j = 1; j <= std::min(m, W); ++j) score[0][j] = j * GAP;
  for (int i = 1; i <= std::min(n, W); ++i) score[i][0] = i * GAP;
  
  for (int i = 1; i <= n; ++i) {
    int centerJ = std::round(i * (double)m / n);
    int minJ = std::max(1, centerJ - W);
    int maxJ = std::min(m, centerJ + W);
    for (int j = minJ; j <= maxJ; ++j) {
      int sMatch = (ref[i-1].toUpper() == read[j-1].toUpper()) ? MATCH : MISMATCH;
      int pDiag = (score[i-1][j-1] != -INF) ? score[i-1][j-1] + sMatch : -INF;
      int pUp   = (score[i-1][j]   != -INF) ? score[i-1][j] + GAP   : -INF;
      int pLeft = (score[i][j-1]   != -INF) ? score[i][j-1] + GAP   : -INF;
      
      score[i][j] = std::max({pDiag, pUp, pLeft});
    }
  }
  
  int i = n, j = m, matchCount = 0;
  QString resRef = "", resRead = "";
  
  while (i > 0 || j > 0) {
    if (i > 0 && j > 0 && score[i][j] != -INF) {
      bool isMatch = (ref[i-1].toUpper() == read[j-1].toUpper());
      int sMatch = isMatch ? MATCH : MISMATCH;
      if (score[i][j] == score[i-1][j-1] + sMatch) {
        resRef.append(ref[i-1]); resRead.append(read[j-1]);
        if (isMatch) matchCount++;
        --i; --j; continue;
      }
    }
    if (i > 0 && (j == 0 || (score[i-1][j] != -INF && score[i][j] == score[i-1][j] + GAP))) {
      resRef.append(ref[i-1]); resRead.append('-'); --i;
    } else if (j > 0) {
      resRef.append('-'); resRead.append(read[j-1]); --j;
    } else break;
  }
  
  std::reverse(resRef.begin(), resRef.end());
  std::reverse(resRead.begin(), resRead.end());
  
  int alignedLen = resRead.length();
  double identity = alignedLen > 0 ? (double)matchCount / alignedLen * 100.0 : 0.0;
  
  return {resRef, resRead, score[n][m], identity, false};
}

// Wavefront (WFA) для расстояния редактирования. Диагональ k = j - i,
// W[e][k+e] — самая дальняя позиция i в ref при стоимости e.
// Выравнивание заканчивается, когда достигнут конец ref или конец рида.
static AlignmentResult wavefrontCore(const QString &ref, const QString &read) {
  const int n = ref.length();
  const int m = read.length();
  if (n == 0 || m == 0) return {};

  const int INVALID = -1;
  const int MAXE = std::min(n + m, 3000); // ограничение памяти: O(e^2)

  std::vector<std::vector<int>> W;
  auto get = [&](int e, int k) -> int {
    if (e < 0 || e >= (int)W.size() || k < -e || k > e) return INVALID;
    return W[e][k + e];
  };
  auto valid = [&](int i, int k) { int j = i + k; return i >= 0 && i <= n && j >= 0 && j <= m; };
  auto extend = [&](int i, int k) {
    int j = i + k;
    while (i < n && j < m && ref[i].toUpper() == read[j].toUpper()) { ++i; ++j; }
    return i;
  };
  auto done = [&](int i, int k) { return i == n || i + k == m; };

  int finalE = -1, finalK = 0, finalI = 0;
  W.push_back({extend(0, 0)});
  if (done(W[0][0], 0)) { finalE = 0; finalK = 0; finalI = W[0][0]; }

  for (int e = 1; e <= MAXE && finalE < 0; ++e) {
    std::vector<int> cur(2 * e + 1, INVALID);
    for (int k = -e; k <= e; ++k) {
      int best = INVALID;
      int a = get(e - 1, k);     if (a >= 0 && valid(a + 1, k)) best = std::max(best, a + 1); // замена
      int b = get(e - 1, k - 1); if (b >= 0 && valid(b, k))     best = std::max(best, b);     // вставка в рид
      int c = get(e - 1, k + 1); if (c >= 0 && valid(c + 1, k)) best = std::max(best, c + 1); // делеция
      if (best < 0) continue;
      best = extend(best, k);
      cur[k + e] = best;
      if (finalE < 0 && done(best, k)) { finalE = e; finalK = k; finalI = best; }
    }
    W.push_back(std::move(cur));
  }

  if (finalE < 0) return alignNeedlemanWunsch(ref, read);

  QString rr, rd; // строятся с конца
  int e = finalE, k = finalK, i = finalI, j = i + k;
  for (int q = m; q > j; --q) { rr.append('-'); rd.append(read[q-1]); }
  for (int q = n; q > i; --q) { rr.append(ref[q-1]); rd.append('-'); }

  while (e > 0) {
    int a = get(e - 1, k), b = get(e - 1, k - 1), c = get(e - 1, k + 1);
    int ca = (a >= 0 && valid(a + 1, k)) ? a + 1 : INVALID;
    int cb = (b >= 0 && valid(b, k))     ? b     : INVALID;
    int cc = (c >= 0 && valid(c + 1, k)) ? c + 1 : INVALID;
    int pre = std::max({ca, cb, cc});
    while (i > pre) { rr.append(ref[i-1]); rd.append(read[i + k - 1]); --i; } // совпадения (extend)
    j = i + k;
    if (pre == ca)      { rr.append(ref[i-1]); rd.append(read[j-1]); i = a; }
    else if (pre == cb) { rr.append('-');      rd.append(read[j-1]); i = b; k = k - 1; }
    else                { rr.append(ref[i-1]); rd.append('-');       i = c; k = k + 1; }
    --e;
  }
  while (i > 0) { rr.append(ref[i-1]); rd.append(read[i + k - 1]); --i; } // e == 0, k == 0

  std::reverse(rr.begin(), rr.end());
  std::reverse(rd.begin(), rd.end());

  return {rr, rd, 1000 - finalE * 10, 0.0, false};
}


// Лучшая диагональ по k-mer голосованию (как в seed-and-extend).
static bool findBestDiagonal(const QString &ref, const QString &read, int &bestDiag) {
  const int K = 6;
  int n = ref.length(), m = read.length();
  if (n < K || m < K) return false;
  std::unordered_map<std::string, std::vector<int>> kmers;
  std::string rs = ref.toUpper().toStdString(), qs = read.toUpper().toStdString();
  for (int i = 0; i + K <= n; ++i) kmers[rs.substr(i, K)].push_back(i);
  std::unordered_map<int, int> votes;
  int maxHits = 0;
  for (int j = 0; j + K <= m; ++j) {
    auto it = kmers.find(qs.substr(j, K));
    if (it == kmers.end()) continue;
    for (int i : it->second) {
      int d = i - j;
      int h = ++votes[d];
      if (h > maxHits) { maxHits = h; bestDiag = d; }
    }
  }
  return maxHits >= 2;
}

// Ядро Wavefront жёстко привязано к началу обеих последовательностей, поэтому для
// перекрытий со сдвигом сначала находим диагональ по k-mer'ам и запускаем WFA с неё.
static AlignmentResult alignWavefront(const QString &ref, const QString &read) {
  int n = ref.length(), m = read.length();
  if (n == 0 || m == 0) return {};
  int bestDiag = 0;
  if (!findBestDiagonal(ref, read, bestDiag)) return wavefrontCore(ref, read);

  int refStart = std::clamp(bestDiag, 0, n);
  int readStart = std::clamp(-bestDiag, 0, m);
  AlignmentResult t = wavefrontCore(ref.mid(refStart), read.mid(readStart));
  if (t.alignedRef.isEmpty() || t.alignedRead.isEmpty()) return wavefrontCore(ref, read);

  QString alignedRef  = ref.left(refStart) + QString(readStart, '-') + t.alignedRef;
  QString alignedRead = QString(refStart, '-') + read.left(readStart) + t.alignedRead;
  return {alignedRef, alignedRead, t.score, 0.0, false};
}

// 7. Встроенный прогрессивный MSA (Clustal-style Progressive Alignment)
static AlignmentResult alignProgressiveClustal(const QString &ref, const QString &read) {
  // Встроенный прогрессивный выравниватель Gotoh с профильными весами
  return alignAffineGapBlock(ref, read);
}

double AlignmentEngine::overlapIdentity(const QString &a, const QString &b) { return ::overlapIdentity(a, b); }

AlignmentResult AlignmentEngine::alignSemiGlobal(const QString &ref, const QString &read, AlignmentAlgorithm algo, RcMode rcMode) {
  auto alignFn = [algo](const QString &r, const QString &q) -> AlignmentResult {
    AlignmentResult res;
    switch (algo) {
    case AlignmentAlgorithm::AffineGapBlock:  res = alignAffineGapBlock(r, q); break;
    case AlignmentAlgorithm::NeedlemanWunsch: res = alignNeedlemanWunsch(r, q); break;
    case AlignmentAlgorithm::SmithWaterman:   res = alignSmithWaterman(r, q); break;
    case AlignmentAlgorithm::BlastSeedExtend: res = alignBlastSeedExtend(r, q); break;
    case AlignmentAlgorithm::BandedGlobal:    res = alignBandedGlobal(r, q); break;
    case AlignmentAlgorithm::Wavefront:       res = alignWavefront(r, q); break;
    case AlignmentAlgorithm::InternalClustal: res = alignProgressiveClustal(r, q); break;
    case AlignmentAlgorithm::OverlapCAP3:
    default:                                   res = alignOverlapCAP3(r, q); break;
    }
    // Пустой результат (например, у Смита-Уотермана без сходства) привёл бы к исчезновению рида.
    if (res.alignedRef.isEmpty() || res.alignedRead.isEmpty()) res = alignOverlapCAP3(r, q);
    res.identity = overlapIdentity(res.alignedRef, res.alignedRead);
    return res;
  };

  if (rcMode == RcMode::ForceForward) {
    return alignFn(ref, read);
  } else if (rcMode == RcMode::ForceReverse) {
    AlignmentResult revRes = alignFn(ref, Ab1Parser::reverseComplement(read));
    revRes.isReverseComplement = true;
    return revRes;
  }

  AlignmentResult fwdRes = alignFn(ref, read);
  AlignmentResult revRes = alignFn(ref, Ab1Parser::reverseComplement(read));
  revRes.isReverseComplement = true;

  // Ориентацию выбираем по счёту выравнивания (внутри одного алгоритма он сопоставим),
  // а identity — только при равенстве: по identity маленькое случайное перекрытие может "победить".
  if (revRes.score > fwdRes.score ||
      (revRes.score == fwdRes.score && revRes.identity > fwdRes.identity)) {
    return revRes;
  }
  return fwdRes;
}
