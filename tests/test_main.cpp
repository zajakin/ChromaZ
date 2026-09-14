#include <QtTest/QtTest>
#include "../ab1parser.h"
#include "../alignmentengine.h"

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
};

QTEST_MAIN(TestChromaZ)
#include "test_main.moc"