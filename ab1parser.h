#ifndef AB1PARSER_H
#define AB1PARSER_H

#include <QString>
#include <vector>

struct Ab1Data {
  QString sequence;
  std::vector<int> traceA;
  std::vector<int> traceC;
  std::vector<int> traceG;
  std::vector<int> traceT;
  std::vector<int> basePositions;
  std::vector<int> qualityScores;
  bool isValid = false;
  QString errorMessage;
};

struct SequenceRecord {
  QString name;
  Ab1Data data;
};

using FastqRecord = SequenceRecord;

class Ab1Parser {
public:
  static Ab1Data parse(const QString &filePath);
  static QString reverseComplement(const QString &seq);
  static Ab1Data getOrientedData(const Ab1Data &orig, bool isRC);
};

class SequenceFileParser {
public:
  static QString parseFastaOrGb(const QString &filePath);
  static std::vector<SequenceRecord> parseFastaOrGbRecords(const QString &filePath);
  static std::vector<SequenceRecord> parseFastq(const QString &filePath);
  static std::vector<SequenceRecord> parseTxt(const QString &filePath);
};

#endif // AB1PARSER_H