#include <QtTest/QtTest>
#include <atomic>
#include "../ab1parser.h"
#include "../alignmentengine.h"
#include "../alignjob.h"
#include "../msa.h"
#include "../tmcalc.h"
#include "../gelmath.h"

namespace {

void put32(QByteArray &b, quint32 v) { b.append(char(v >> 24)); b.append(char(v >> 16)); b.append(char(v >> 8)); b.append(char(v)); }
void put16(QByteArray &b, quint32 v) { b.append(char(v >> 8)); b.append(char(v)); }

// Синтетический SCF (v2: чередующиеся сэмплы и 12-байтовые базы; v3: планарные дельта-сэмплы и раздельные массивы).
struct ScfSample {
  QString bases = "ACGT";
  std::vector<int> peaks = {5, 15, 25, 35};
  std::vector<int> prob = {20, 30, 40, 50};          // вероятность вызванной базы
  std::vector<int> trace[4];                         // A, C, G, T
  ScfSample() {
    for (int ch = 0; ch < 4; ++ch)
      for (int i = 0; i < 40; ++i) trace[ch].push_back((i * 7 + ch * 13) % 200 + ch);
  }
};

QByteArray buildScf(bool v3, const ScfSample &s) {
  const int S = 40, B = s.bases.length();
  const quint32 samplesOff = 128, samplesBytes = S * 4 * 2;
  const quint32 basesOff = samplesOff + samplesBytes;
  const quint32 basesBytes = B * 12;   // v2 и v3 занимают одинаковый объём
  QByteArray b;
  put32(b, 0x2e736366); put32(b, S); put32(b, samplesOff); put32(b, B);
  put32(b, 0); put32(b, 0); put32(b, basesOff);
  put32(b, 0); put32(b, basesOff + basesBytes);
  b.append(v3 ? "3.00" : "2.00");
  put32(b, 2);                       // sample_size
  while (b.size() < 128) b.append(char(0));

  if (v3) {
    for (int ch = 0; ch < 4; ++ch) {
      std::vector<int> d(S);
      int prev = 0;
      for (int i = 0; i < S; ++i) { d[i] = (s.trace[ch][i] - prev) & 0xFFFF; prev = s.trace[ch][i]; }
      int prev2 = 0;
      for (int i = 0; i < S; ++i) { int v = (d[i] - prev2) & 0xFFFF; prev2 = d[i]; put16(b, v); }
    }
    for (int p : s.peaks) put32(b, p);
    for (int ch = 0; ch < 4; ++ch)
      for (int i = 0; i < B; ++i) b.append(char("ACGT"[ch] == s.bases[i].toLatin1() ? s.prob[i] : 3));
    for (int i = 0; i < B; ++i) b.append(s.bases[i].toLatin1());
    for (int i = 0; i < 3 * B; ++i) b.append(char(0));
  } else {
    for (int i = 0; i < S; ++i)
      for (int ch = 0; ch < 4; ++ch) put16(b, s.trace[ch][i]);
    for (int i = 0; i < B; ++i) {
      put32(b, s.peaks[i]);
      for (int ch = 0; ch < 4; ++ch) b.append(char("ACGT"[ch] == s.bases[i].toLatin1() ? s.prob[i] : 3));
      b.append(s.bases[i].toLatin1());
      b.append(3, char(0));
    }
  }
  return b;
}

QString writeTemp(QTemporaryDir &dir, const QString &name, const QByteArray &data) {
  QString path = dir.filePath(name);
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly)) return QString();
  f.write(data);
  return path;
}

QString randomDna(unsigned seed, int n) {
  QString s;
  unsigned x = seed;
  for (int i = 0; i < n; ++i) { x = x * 1103515245u + 12345u; s += QLatin1Char("ACGT"[(x >> 16) & 3]); }
  return s;
}

} // namespace

class TestChromaZ : public QObject {
  Q_OBJECT
  
  private slots:
  void testReverseComplement() {
    QString original = "ATGCAG";
    QString expected = "CTGCAT";
    QCOMPARE(Ab1Parser::reverseComplement(original), expected);
    QCOMPARE(Ab1Parser::reverseComplement("a-t-g-c"), "G-C-A-T");
  }
  
  void testOrientedDataRC() {
    Ab1Data orig;
    orig.sequence = "ATGC";
    orig.qualityScores = {10, 20, 30, 40};
    orig.traceA = {100, 0, 0, 0};
    orig.traceC = {0, 100, 0, 0};
    orig.traceG = {0, 0, 100, 0};
    orig.traceT = {0, 0, 0, 100};
    orig.basePositions = {0, 1, 2, 3};
    
    Ab1Data oriented = Ab1Parser::getOrientedData(orig, true);
    
    QCOMPARE(oriented.sequence, "GCAT");
    QCOMPARE(oriented.qualityScores[0], 40);
    QCOMPARE(oriented.qualityScores[3], 10);
  }
  
  void testAlignmentExactMatch() {
    QString ref  = "ATGCGATCGATCG";
    QString read = "ATGCGATCGATCG";
    
    AlignmentResult res = AlignmentEngine::alignSemiGlobal(ref, read);
    
    QCOMPARE(res.identity, 100.0);
    QCOMPARE(res.isReverseComplement, false);
    QCOMPARE(res.alignedRead, read);
  }
  
  void testAlignmentAutoReverseComplement() {
    QString ref  = "ATGCGATCGATCG";
    QString read = Ab1Parser::reverseComplement(ref);
    
    AlignmentResult res = AlignmentEngine::alignSemiGlobal(ref, read);
    
    QCOMPARE(res.isReverseComplement, true);
    QVERIFY(res.identity > 95.0);
  }
  
  void testFastqParsing() {
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    
    QTextStream out(&tempFile);
    out << "@Read1\nATGC\n+\nIIII\n"; // ASCII 'I' = 73 - 33 = 40 (Phred40)
    out.flush();
    
    auto records = SequenceFileParser::parseFastq(tempFile.fileName());
    QCOMPARE((int)records.size(), 1);
    QCOMPARE(records[0].name, "Read1");
    QCOMPARE(records[0].data.sequence, "ATGC");
    QCOMPARE(records[0].data.qualityScores[0], 40);
  }

  // ---------- SCF ----------
  void testScfV3() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    ScfSample s;
    QString path = writeTemp(dir, "t3.scf", buildScf(true, s));
    QVERIFY(!path.isEmpty());
    Ab1Data d = Ab1Parser::parse(path);
    QVERIFY2(d.isValid, qPrintable(d.errorMessage));
    QCOMPARE(d.sequence, QStringLiteral("ACGT"));
    QCOMPARE(d.basePositions, s.peaks);
    QCOMPARE(d.qualityScores, s.prob);
    QCOMPARE(d.traceA, s.trace[0]);
    QCOMPARE(d.traceC, s.trace[1]);
    QCOMPARE(d.traceG, s.trace[2]);
    QCOMPARE(d.traceT, s.trace[3]);
  }

  void testScfV2() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    ScfSample s;
    QString path = writeTemp(dir, "t2.scf", buildScf(false, s));
    Ab1Data d = Ab1Parser::parse(path);
    QVERIFY2(d.isValid, qPrintable(d.errorMessage));
    QCOMPARE(d.sequence, QStringLiteral("ACGT"));
    QCOMPARE(d.basePositions, s.peaks);
    QCOMPARE(d.qualityScores, s.prob);
    QCOMPARE(d.traceG, s.trace[2]);
  }

  void testScfTruncatedIsRejected() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    ScfSample s;
    QByteArray b = buildScf(true, s);
    b.truncate(200);
    Ab1Data d = Ab1Parser::parse(writeTemp(dir, "bad.scf", b));
    QVERIFY(!d.isValid);
  }

  // ---------- GenBank ----------
  void testGenBankWithFeatures() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    QByteArray gb =
      "LOCUS       TESTSEQ                   20 bp    DNA     linear   SYN 01-JAN-2026\n"
      "DEFINITION  Synthetic construct with FEATURES words ACGT.\n"
      "FEATURES             Location/Qualifiers\n"
      "     source          1..20\n"
      "                     /organism=\"Homo sapiens\"\n"
      "     CDS             1..18\n"
      "                     /product=\"agct protein\"\n"
      "ORIGIN\n"
      "        1 atgcatgcat gcatgcatgc\n"
      "//\n"
      "LOCUS       SECOND                    8 bp    DNA     linear   SYN 01-JAN-2026\n"
      "ORIGIN\n"
      "        1 ggggcccc\n"
      "//\n";
    auto recs = SequenceFileParser::parseFastaOrGbRecords(writeTemp(dir, "t.gb", gb));
    QCOMPARE((int)recs.size(), 2);
    QCOMPARE(recs[0].name, QStringLiteral("TESTSEQ"));
    QCOMPARE(recs[0].data.sequence, QStringLiteral("ATGCATGCATGCATGCATGC"));
    QCOMPARE(recs[1].data.sequence, QStringLiteral("GGGGCCCC"));
  }

  void testMultiFasta() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    auto recs = SequenceFileParser::parseFastaOrGbRecords(writeTemp(dir, "m.fa", ">a desc\nACGT\nAC\n>b\nGGTT\n"));
    QCOMPARE((int)recs.size(), 2);
    QCOMPARE(recs[0].name, QStringLiteral("a"));
    QCOMPARE(recs[0].data.sequence, QStringLiteral("ACGTAC"));
    QCOMPARE(recs[1].data.sequence, QStringLiteral("GGTT"));
  }

  // ---------- удалённые основания: данные хроматограммы сохраняются ----------
  void testDeletedBasesSurviveRealignment() {
    const QString g = randomDna(11, 900);
    QString r1 = g.mid(0, 600), r2 = g.mid(200, 600), r3 = g.mid(300, 600);
    r2[100] = ' '; r2[101] = ' ';                          // две удалённые базы в середине рида
    QString r4 = Ab1Parser::reverseComplement(g.mid(100, 500));
    r4[250] = ' ';                                         // удаление в обратно-комплементарном риде

    for (int algo : {0, 1, 4, 6, 7}) {
      AlignJobInput in;
      in.algo = static_cast<AlignmentAlgorithm>(algo);
      in.threads = 2;
      for (const QString &s : {r1, r2, r3, r4}) { AlignJobTrackIn t; t.rawSeq = s; in.tracks.push_back(t); }
      std::atomic<bool> cancel{false};
      AlignJobOutput out = runAlignJob(in, cancel);
      QVERIFY(!out.cancelled);
      QCOMPARE((int)out.tracks.size(), 4);
      for (int k = 0; k < 4; ++k) {
        const QString raw = in.tracks[k].rawSeq;
        const QString oriented = out.tracks[k].isRC ? Ab1Parser::reverseComplement(raw) : raw;
        QString noGaps = out.tracks[k].alignedRead;
        noGaps.remove('-');
        // каждый символ рида (включая ' ') присутствует ровно один раз и в прежнем порядке
        QCOMPARE(noGaps, oriented);
        QCOMPARE(out.tracks[k].alignedRef.length(), out.tracks[k].alignedRead.length());
      }
      QVERIFY(out.tracks[3].isRC);
      MasterAlignment m = buildMaster(out.reference, out.tracks);
      for (const QString &row : m.masterReads) QCOMPARE(row.length(), m.masterRef.length());
    }
  }

  void testReinsertDeletedBasesPrefersExistingGap() {
    QString ref = "ACGTAACGT", read = "ACGT-ACGT";
    // удалённое основание после 4-й базы: должно занять существующий гэп, а не добавлять колонку
    reinsertDeletedBases(ref, read, QStringLiteral("ACGT ACGT"));
    QCOMPARE(read, QStringLiteral("ACGT ACGT"));
    QCOMPARE(ref, QStringLiteral("ACGTAACGT"));
    QString ref2 = "ACGTACGT", read2 = "ACGTACGT";
    reinsertDeletedBases(ref2, read2, QStringLiteral("ACGT ACGT"));
    QCOMPARE(read2, QStringLiteral("ACGT ACGT"));
    QCOMPARE(ref2, QStringLiteral("ACGT-ACGT"));
  }

  void testGapsInSourceSequenceDoNotShiftDeletedBases() {
    // Рид из FASTA с гэпами ('-') и удалённым основанием: гэпы не входят в выравнивание
    // и не должны сдвигать позицию пустого слота.
    const QString g = randomDna(3, 500);
    QString r = g.mid(100, 300);
    r.insert(50, QStringLiteral("--"));
    r[120] = ' ';
    AlignJobInput in;
    AlignJobTrackIn a, b;
    a.rawSeq = g; b.rawSeq = r;
    in.tracks = {a, b};
    std::atomic<bool> cancel{false};
    AlignJobOutput out = runAlignJob(in, cancel);
    QVERIFY(!out.cancelled);
    QString noGaps = out.tracks[1].alignedRead;
    noGaps.remove('-');
    QString expected = r;
    expected.remove('-');
    QCOMPARE(noGaps, expected);
    QCOMPARE(noGaps.indexOf(QLatin1Char(' ')), 118);   // позиция слота не сместилась из-за двух '-' в начале
  }

  void testRefinementProducesConsensusReference() {
    const QString g = randomDna(5, 700);
    AlignJobInput in;
    in.refineIterations = 2;
    for (int st : {0, 100, 200}) { AlignJobTrackIn t; t.rawSeq = g.mid(st, 450); in.tracks.push_back(t); }
    std::atomic<bool> cancel{false};
    AlignJobOutput out = runAlignJob(in, cancel);
    QVERIFY(out.autoReference);
    QCOMPARE(out.reference, g.mid(0, 650));
  }

  void testCancelledJob() {
    AlignJobInput in;
    for (int i = 0; i < 4; ++i) { AlignJobTrackIn t; t.rawSeq = randomDna(i + 1, 400); in.tracks.push_back(t); }
    std::atomic<bool> cancel{true};
    QVERIFY(runAlignJob(in, cancel).cancelled);
  }

  // ---------- прогрессивное MSA ----------
  void testProgressiveMsa() {
    const std::string g = randomDna(7, 900).toStdString();
    std::vector<std::string> reads;
    for (int k = 0; k < 8; ++k) {
      std::string r = g.substr((k * 37) % 300, 400 + (k * 23) % 200);
      if (k % 2) r.erase(100 + k, 1);
      if (k % 3 == 0) r.insert(200, "A");
      reads.push_back(r);
    }
    auto rows = Msa::progressiveAlign(reads);
    QCOMPARE(rows.size(), reads.size());
    for (size_t i = 0; i < rows.size(); ++i) {
      QCOMPARE(rows[i].size(), rows[0].size());
      std::string ungapped;
      for (char c : rows[i]) if (c != '-') ungapped += c;
      QVERIFY(ungapped == reads[i]);
    }
  }

  // ---------- Tm ----------
  void testTmFormulas() {
    QCOMPARE(TmCalc::wallace("ATGCATGC"), 24.0);
    QVERIFY(qAbs(TmCalc::gcFormula("ATGCATGCATGCATGCATGC") - 51.78) < 0.01);
  }

  void testNearestNeighborTm() {
    const std::string p = "GCGTAACGGCTAGTCAAGCT";
    auto low = TmCalc::nearestNeighbor(p, 50, 0, 250);
    auto high = TmCalc::nearestNeighbor(p, 150, 0, 250);
    QVERIFY(low.valid && high.valid);
    QVERIFY(high.tm > low.tm);                 // больше соли — выше Tm
    QVERIFY(low.tm > 40 && low.tm < 75);
    QVERIFY(low.deltaG37 < 0);
    QVERIFY(!TmCalc::nearestNeighbor("GCGTAANCGG", 50, 0, 250).valid);   // неоднозначные основания
    QVERIFY(!TmCalc::nearestNeighbor(p, 0, 0, 250).valid);               // соль не задана
    QVERIFY(!TmCalc::nearestNeighbor(p, 50, 0, 0).valid);                // праймер не задан
    QVERIFY(TmCalc::nearestNeighbor("CGCGAATTCGCG", 50, 0, 250).selfComplementary);
  }

  // ---------- виртуальный гель ----------
  void testFragmentSizes() {
    QCOMPARE(fragmentSizes(1000, {300, 700}, false), (std::vector<int>{400, 300, 300}));
    QCOMPARE(fragmentSizes(1000, {300, 700}, true), (std::vector<int>{600, 400}));
    QCOMPARE(fragmentSizes(1000, {}, false), (std::vector<int>{1000}));
    QCOMPARE(fragmentSizes(1000, {500}, true), (std::vector<int>{1000}));
    QCOMPARE(fragmentSizes(1000, {0, 1000, 250}, false), (std::vector<int>{750, 250}));
  }
};

QTEST_MAIN(TestChromaZ)
#include "test_main.moc"
