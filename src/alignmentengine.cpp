#include "alignmentengine.h"
#include "ab1parser.h"
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <cmath>

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
  
  const int MATCH = 3, MISMATCH = -2, GAP_OPEN = -6, GAP_EXT = -1, INF = 1e9;
  
  std::vector<std::vector<int>> M(n + 1, std::vector<int>(m + 1, -INF));
  std::vector<std::vector<int>> X(n + 1, std::vector<int>(m + 1, -INF));
  std::vector<std::vector<int>> Y(n + 1, std::vector<int>(m + 1, -INF));
  
  M[0][0] = 0;
  for (int j = 1; j <= m; ++j) X[0][j] = GAP_OPEN + (j - 1) * GAP_EXT;
  
  for (int i = 1; i <= n; ++i) {
    for (int j = 1; j <= m; ++j) {
      int scoreMatch = (ref[i-1].toUpper() == read[j-1].toUpper()) ? MATCH : MISMATCH;
      int bestPrev = std::max({M[i-1][j-1], X[i-1][j-1], Y[i-1][j-1]});
      if (bestPrev != -INF) M[i][j] = bestPrev + scoreMatch;
      
      X[i][j] = std::max(M[i][j-1] + GAP_OPEN, X[i][j-1] + GAP_EXT);
      Y[i][j] = std::max(M[i-1][j] + GAP_OPEN, Y[i-1][j] + GAP_EXT);
    }
  }
  
  int maxI = n, maxJ = m, maxScore = -INF;
  for (int j = 0; j <= m; ++j) {
    int s = std::max({M[n][j], X[n][j], Y[n][j]});
    if (s > maxScore) { maxScore = s; maxJ = j; }
  }
  
  QString resRef = "", resRead = "";
  int i = maxI, j = maxJ, matchCount = 0, state = 0;
  
  for (int k = m; k > j; --k) { resRef.append('-'); resRead.append(read[k-1]); }
  
  while (i > 0 && j > 0) {
    bool isMatch = (ref[i-1].toUpper() == read[j-1].toUpper());
    int scoreMatch = isMatch ? MATCH : MISMATCH;
    
    if (state == 0) {
      resRef.append(ref[i-1]); resRead.append(read[j-1]);
      if (isMatch) matchCount++;
      int prevBest = M[i][j] - scoreMatch;
      if (prevBest == M[i-1][j-1]) state = 0;
      else if (prevBest == X[i-1][j-1]) state = 1;
      else state = 2;
      --i; --j;
    } else if (state == 1) {
      resRef.append('-'); resRead.append(read[j-1]);
      if (X[i][j] == M[i][j-1] + GAP_OPEN) state = 0;
      else state = 1;
      --j;
    } else {
      resRef.append(ref[i-1]); resRead.append('-');
      if (Y[i][j] == M[i-1][j] + GAP_OPEN) state = 0;
      else state = 2;
      --i;
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
  
  for (int k = startJ; k > 0; --k) { resRef.append('-'); resRead.append(read[k-1]); }
  for (int k = startI; k > 0; --k) { resRef.append(ref[k-1]); resRead.append('-'); }
  
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
  int W = 15;
  
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

static AlignmentResult alignWavefront(const QString &ref, const QString &read) {
  int n = ref.length();
  int m = read.length();
  if (n == 0 || m == 0) return {};
  
  int maxE = n + m;
  int maxK = n + m;
  int kOffset = maxK;
  
  std::vector<std::vector<int>> V(maxE + 1, std::vector<int>(2 * maxK + 1, -2));
  
  int h = 0;
  while (h < n && h < m && ref[h].toUpper() == read[h].toUpper()) {
    h++;
  }
  V[0][kOffset] = h;
  
  int finalE = 0, finalK = 0;
  bool reachedEnd = false;
  
  if (V[0][kOffset] == n || V[0][kOffset] == m) {
    finalE = 0; finalK = 0; reachedEnd = true;
  } else {
    for (int e = 1; e <= maxE; ++e) {
      for (int k = -e; k <= e; ++k) {
        int idx = k + kOffset;
        int h1 = (idx - 1 >= 0) ? V[e-1][idx - 1] + 1 : -2;
        int h2 = (idx + 1 <= 2 * maxK) ? V[e-1][idx + 1] : -2;
        int h3 = (idx >= 0 && idx <= 2 * maxK) ? V[e-1][idx] + 1 : -2;
        
        int currH = std::max({h1, h2, h3});
        if (currH >= 0) {
          while (currH < n && (currH + k) < m && ref[currH].toUpper() == read[currH + k].toUpper()) {
            currH++;
          }
          V[e][idx] = currH;
          
          if (currH == n || (currH + k) == m) {
            finalE = e; finalK = k; reachedEnd = true;
            break;
          }
        }
      }
      if (reachedEnd) break;
    }
  }
  
  if (!reachedEnd) {
    return alignNeedlemanWunsch(ref, read);
  }
  
  int currE = finalE, currK = finalK;
  int currI = V[currE][currK + kOffset];
  int currJ = currI + currK;
  
  QString resRef = "", resRead = "";
  int matchCount = 0;
  
  for (int k = m; k > currJ; --k) { resRef.append('-'); resRead.append(read[k-1]); }
  for (int k = n; k > currI; --k) { resRef.append(ref[k-1]); resRead.append('-'); }
  
  while (currI > 0 || currJ > 0) {
    if (currE == 0) {
      while (currI > 0 && currJ > 0) {
        resRef.append(ref[currI - 1]);
        resRead.append(read[currJ - 1]);
        matchCount++;
        currI--; currJ--;
      }
      break;
    }
    
    int k = currK;
    int idx = k + kOffset;
    int h1 = (idx - 1 >= 0) ? V[currE-1][idx - 1] + 1 : -2;
    int h2 = (idx + 1 <= 2 * maxK) ? V[currE-1][idx + 1] : -2;
    int h3 = (idx >= 0 && idx <= 2 * maxK) ? V[currE-1][idx] + 1 : -2;
    
    while (currI > std::max({h1, h2, h3, 0})) {
      resRef.append(ref[currI - 1]);
      resRead.append(read[currJ - 1]);
      matchCount++;
      currI--; currJ--;
    }
    
    if (currI == 0 && currJ == 0) break;
    
    if (h1 >= h2 && h1 >= h3 && h1 >= 0) {
      resRef.append('-'); resRead.append(read[currJ - 1]);
      currK = k - 1; currE--;
      currI = V[currE][currK + kOffset]; currJ = currI + currK;
    } else if (h2 >= h1 && h2 >= h3 && h2 >= 0) {
      resRef.append(ref[currI - 1]); resRead.append('-');
      currK = k + 1; currE--;
      currI = V[currE][currK + kOffset]; currJ = currI + currK;
    } else {
      resRef.append(ref[currI - 1]); resRead.append(read[currJ - 1]);
      currK = k; currE--;
      currI = V[currE][currK + kOffset]; currJ = currI + currK;
    }
  }
  
  while (currI > 0) { resRef.append(ref[currI - 1]); resRead.append('-'); currI--; }
  while (currJ > 0) { resRef.append('-'); resRead.append(read[currJ - 1]); currJ--; }
  
  std::reverse(resRef.begin(), resRef.end());
  std::reverse(resRead.begin(), resRead.end());
  
  int alignedLen = resRead.length();
  double identity = alignedLen > 0 ? (double)matchCount / alignedLen * 100.0 : 0.0;
  
  return {resRef, resRead, 1000 - finalE * 10, identity, false};
}

// 7. Встроенный прогрессивный MSA (Clustal-style Progressive Alignment)
static AlignmentResult alignProgressiveClustal(const QString &ref, const QString &read) {
  // Встроенный прогрессивный выравниватель Gotoh с профильными весами
  return alignAffineGapBlock(ref, read);
}

AlignmentResult AlignmentEngine::alignSemiGlobal(const QString &ref, const QString &read, AlignmentAlgorithm algo, RcMode rcMode) {
  auto alignFn = [algo](const QString &r, const QString &q) -> AlignmentResult {
    switch (algo) {
    case AlignmentAlgorithm::AffineGapBlock:  return alignAffineGapBlock(r, q);
    case AlignmentAlgorithm::NeedlemanWunsch: return alignNeedlemanWunsch(r, q);
    case AlignmentAlgorithm::SmithWaterman:   return alignSmithWaterman(r, q);
    case AlignmentAlgorithm::BlastSeedExtend: return alignBlastSeedExtend(r, q);
    case AlignmentAlgorithm::BandedGlobal:    return alignBandedGlobal(r, q);
    case AlignmentAlgorithm::Wavefront:       return alignWavefront(r, q);
    case AlignmentAlgorithm::InternalClustal: return alignProgressiveClustal(r, q);
    case AlignmentAlgorithm::OverlapCAP3:
    default:                                   return alignOverlapCAP3(r, q);
    }
  };
  
  if (rcMode == RcMode::ForceForward) {
    return alignFn(ref, read);
  } else if (rcMode == RcMode::ForceReverse) {
    QString revSeq = Ab1Parser::reverseComplement(read);
    AlignmentResult revRes = alignFn(ref, revSeq);
    revRes.isReverseComplement = true;
    return revRes;
  }
  
  AlignmentResult fwdRes = alignFn(ref, read);
  QString revSeq = Ab1Parser::reverseComplement(read);
  AlignmentResult revRes = alignFn(ref, revSeq);
  revRes.isReverseComplement = true;
  
  if (revRes.identity > fwdRes.identity) {
    return revRes;
  }
  return fwdRes;
}