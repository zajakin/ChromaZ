#ifndef ALIGNMENTENGINE_H
#define ALIGNMENTENGINE_H

#include <QString>

enum class AlignmentAlgorithm {
  OverlapCAP3     = 0, // OLC / CAP3 Sanger Overlap (Vector NTI style)
    AffineGapBlock  = 1, // Gotoh Semi-Global (с аффинным штрафом)
    NeedlemanWunsch = 2, // Needleman-Wunsch Global (строгое глобальное)
    SmithWaterman   = 3, // Smith-Waterman Local (честное локальное)
    BlastSeedExtend = 4, // BLAST (k-mer Seed & Extend)
    BandedGlobal    = 5, // Banded DP (полосовое)
    Wavefront       = 6, // Wavefront (WFA)
    InternalClustal = 7  // Встроенный многосторонний Clustal-Style Progressive MSA
};

class AlignmentEngine {
public:
  enum class RcMode {
    Auto,
    ForceForward,
    ForceReverse
  };
  
  struct AlignmentResult {
    QString alignedRef;
    QString alignedRead;
    int score = 0;
    double identity = 0.0;
    bool isReverseComplement = false;
  };
  
  static AlignmentResult alignSemiGlobal(const QString &ref, const QString &read, 
                                         AlignmentAlgorithm algo = AlignmentAlgorithm::OverlapCAP3,
                                         RcMode rcMode = RcMode::Auto);
};

using AlignmentResult = AlignmentEngine::AlignmentResult;

#endif // ALIGNMENTENGINE_H