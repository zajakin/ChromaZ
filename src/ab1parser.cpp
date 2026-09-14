#include "ab1parser.h"
#include <QFile>
#include <QDataStream>
#include <QTextStream>
#include <cstring>
#include <algorithm>

struct DirEntry {
  char tag[4];
  uint32_t number;
  uint16_t elemType;
  uint16_t elemSize;
  uint32_t numElems;
  uint32_t dataSize;
  uint32_t dataOffset;
};

struct ScfHeader {
  uint32_t magic;
  uint32_t samples;
  uint32_t samples_offset;
  uint32_t bases;
  uint32_t bases_left_clip;
  uint32_t bases_right_clip;
  uint32_t bases_offset;
  uint32_t comments_size;
  uint32_t comments_offset;
  char version[4];
  uint32_t sample_size;
};

QString Ab1Parser::reverseComplement(const QString &seq) {
  QString res = "";
  for (int i = seq.length() - 1; i >= 0; --i) {
    QChar c = seq[i].toUpper();
    if (c == 'A') res += 'T';
    else if (c == 'T') res += 'A';
    else if (c == 'C') res += 'G';
    else if (c == 'G') res += 'C';
    else res += c;
  }
  return res;
}

Ab1Data Ab1Parser::getOrientedData(const Ab1Data &orig, bool isRC) {
  if (!isRC) return orig;
  
  Ab1Data res;
  res.isValid = orig.isValid;
  res.errorMessage = orig.errorMessage;
  res.sequence = reverseComplement(orig.sequence);
  
  res.qualityScores = orig.qualityScores;
  std::reverse(res.qualityScores.begin(), res.qualityScores.end());
  
  res.traceA = orig.traceT; std::reverse(res.traceA.begin(), res.traceA.end());
  res.traceT = orig.traceA; std::reverse(res.traceT.begin(), res.traceT.end());
  res.traceC = orig.traceG; std::reverse(res.traceC.begin(), res.traceC.end());
  res.traceG = orig.traceC; std::reverse(res.traceG.begin(), res.traceG.end());
  
  int sMax = 0;
  if (!orig.traceA.empty()) sMax = std::max(sMax, (int)orig.traceA.size());
  if (!orig.traceC.empty()) sMax = std::max(sMax, (int)orig.traceC.size());
  if (!orig.traceG.empty()) sMax = std::max(sMax, (int)orig.traceG.size());
  if (!orig.traceT.empty()) sMax = std::max(sMax, (int)orig.traceT.size());
  
  res.basePositions.reserve(orig.basePositions.size());
  for (int i = (int)orig.basePositions.size() - 1; i >= 0; --i) {
    int p = orig.basePositions[i];
    int pRev = (sMax > 0) ? (sMax - 1 - p) : p;
    res.basePositions.push_back(std::max(0, pRev));
  }
  
  return res;
}

Ab1Data Ab1Parser::parseScf(const QString &filePath) {
  Ab1Data data;
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    data.errorMessage = "Failed to open SCF file";
    return data;
  }
  
  QDataStream in(&file);
  in.setByteOrder(QDataStream::BigEndian);
  
  ScfHeader hdr;
  in >> hdr.magic >> hdr.samples >> hdr.samples_offset >> hdr.bases 
     >> hdr.bases_left_clip >> hdr.bases_right_clip >> hdr.bases_offset
     >> hdr.comments_size >> hdr.comments_offset;
     in.readRawData(hdr.version, 4);
     in >> hdr.sample_size;
     
     if (hdr.magic != 0x2e736366) { // ".scf"
       data.errorMessage = "Invalid SCF format";
       return data;
     }
     
     file.seek(hdr.samples_offset);
     uint32_t numSamples = hdr.samples;
     data.traceA.resize(numSamples);
     data.traceC.resize(numSamples);
     data.traceG.resize(numSamples);
     data.traceT.resize(numSamples);
     
     for (uint32_t i = 0; i < numSamples; ++i) {
       if (hdr.sample_size == 1) {
         uint8_t a, c, g, t; in >> a >> c >> g >> t;
         data.traceA[i] = a; data.traceC[i] = c; data.traceG[i] = g; data.traceT[i] = t;
       } else {
         uint16_t a, c, g, t; in >> a >> c >> g >> t;
         data.traceA[i] = a; data.traceC[i] = c; data.traceG[i] = g; data.traceT[i] = t;
       }
     }
     
     file.seek(hdr.bases_offset);
     for (uint32_t i = 0; i < hdr.bases; ++i) {
       uint32_t pos; in >> pos;
       data.basePositions.push_back(pos);
     }
     
     file.seek(hdr.bases_offset + hdr.bases * 12);
     QByteArray seqBytes = file.read(hdr.bases);
     data.sequence = QString::fromLatin1(seqBytes).toUpper();
     
     for (uint32_t i = 0; i < hdr.bases; ++i) {
       data.qualityScores.push_back(30);
     }
     
     data.isValid = true;
     return data;
}

Ab1Data Ab1Parser::parse(const QString &filePath) {
  if (filePath.endsWith(".scf", Qt::CaseInsensitive)) {
    return parseScf(filePath);
  }
  
  Ab1Data data;
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    data.errorMessage = "Failed to open file";
    return data;
  }
  
  QDataStream in(&file);
  in.setByteOrder(QDataStream::BigEndian);
  
  char magic[4];
  in.readRawData(magic, 4);
  if (std::memcmp(magic, "ABIF", 4) != 0) {
    return parseScf(filePath);
  }
  
  uint16_t version;
  in >> version;
  
  DirEntry rootDir;
  in.readRawData(rootDir.tag, 4);
  in >> rootDir.number >> rootDir.elemType >> rootDir.elemSize 
     >> rootDir.numElems >> rootDir.dataSize >> rootDir.dataOffset;
  
  file.seek(rootDir.dataOffset);
  
  std::vector<DirEntry> entries;
  for (uint32_t i = 0; i < rootDir.numElems; ++i) {
    DirEntry e;
    in.readRawData(e.tag, 4);
    in >> e.number >> e.elemType >> e.elemSize 
       >> e.numElems >> e.dataSize >> e.dataOffset;
    uint32_t handle; in >> handle;
    entries.push_back(e);
  }
  
  auto readShortArray = [&](const char tag[4], uint32_t num) -> std::vector<int> {
    std::vector<int> result;
    for (const auto &e : entries) {
      if (std::memcmp(e.tag, tag, 4) == 0 && e.number == num) {
        file.seek(e.dataOffset);
        for (uint32_t k = 0; k < e.numElems; ++k) {
          uint16_t val; in >> val;
          result.push_back(val);
        }
        break;
      }
    }
    return result;
  };
  
  QString fwo = "ACGT"; 
  for (const auto &e : entries) {
    if (std::memcmp(e.tag, "FWO_", 4) == 0 && e.number == 1) {
      char chars[4];
      chars[0] = (e.dataOffset >> 24) & 0xFF;
      chars[1] = (e.dataOffset >> 16) & 0xFF;
      chars[2] = (e.dataOffset >> 8) & 0xFF;
      chars[3] = e.dataOffset & 0xFF;
      fwo = QString::fromLatin1(chars, 4);
      break;
    }
  }
  
  std::vector<int> d9  = readShortArray("DATA", 9);
  std::vector<int> d10 = readShortArray("DATA", 10);
  std::vector<int> d11 = readShortArray("DATA", 11);
  std::vector<int> d12 = readShortArray("DATA", 12);
  
  std::vector<std::vector<int>*> traces = {&d9, &d10, &d11, &d12};
  for (int i = 0; i < 4 && i < fwo.length(); ++i) {
    QChar base = fwo[i].toUpper();
    if (base == 'A') data.traceA = *traces[i];
    else if (base == 'C') data.traceC = *traces[i];
    else if (base == 'G') data.traceG = *traces[i];
    else if (base == 'T') data.traceT = *traces[i];
  }
  
  for (const auto &e : entries) {
    if (std::memcmp(e.tag, "PBAS", 4) == 0 && (e.number == 1 || e.number == 2)) {
      file.seek(e.dataOffset);
      QByteArray seqData = file.read(e.numElems);
      data.sequence = QString::fromLatin1(seqData);
      if (!data.sequence.isEmpty()) break;
    }
  }
  
  data.basePositions = readShortArray("PLOC", 1);
  if (data.basePositions.empty()) data.basePositions = readShortArray("PLOC", 2);
  
  for (const auto &e : entries) {
    if (std::memcmp(e.tag, "PCON", 4) == 0 && (e.number == 1 || e.number == 2)) {
      file.seek(e.dataOffset);
      QByteArray qData = file.read(e.numElems);
      for (char b : qData) {
        data.qualityScores.push_back(static_cast<uint8_t>(b));
      }
      if (!data.qualityScores.empty()) break;
    }
  }
  
  data.isValid = true;
  return data;
}

QString SequenceFileParser::parseFastaOrGb(const QString &filePath) {
  auto records = parseFastaOrGbRecords(filePath);
  if (!records.empty()) return records[0].data.sequence;
  return "";
}

std::vector<SequenceRecord> SequenceFileParser::parseFastaOrGbRecords(const QString &filePath) {
  std::vector<SequenceRecord> records;
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return records;
  
  QTextStream in(&file);
  QString currentName = "";
  QString currentSeq = "";
  
  while (!in.atEnd()) {
    QString line = in.readLine().trimmed();
    if (line.isEmpty()) continue;
    
    if (line.startsWith('>')) {
      if (!currentSeq.isEmpty()) {
        Ab1Data d; d.sequence = currentSeq; d.isValid = true;
        records.push_back({currentName.isEmpty() ? "Sequence" : currentName, d});
        currentSeq.clear();
      }
      currentName = line.mid(1).split(' ').first();
    } else if (line.startsWith("LOCUS")) {
      if (!currentSeq.isEmpty()) {
        Ab1Data d; d.sequence = currentSeq; d.isValid = true;
        records.push_back({currentName.isEmpty() ? "Sequence" : currentName, d});
        currentSeq.clear();
      }
      QStringList parts = line.split(' ', Qt::SkipEmptyParts);
      if (parts.size() > 1) currentName = parts[1];
    } else if (line.startsWith("ORIGIN")) {
      continue;
    } else if (line.startsWith("//")) {
      if (!currentSeq.isEmpty()) {
        Ab1Data d; d.sequence = currentSeq; d.isValid = true;
        records.push_back({currentName.isEmpty() ? "Sequence" : currentName, d});
        currentSeq.clear();
        currentName.clear();
      }
    } else {
      for (QChar c : line) {
        if (c.isLetter() || c == '-') currentSeq += c.toUpper();
      }
    }
  }
  
  if (!currentSeq.isEmpty()) {
    Ab1Data d; d.sequence = currentSeq; d.isValid = true;
    records.push_back({currentName.isEmpty() ? "Sequence" : currentName, d});
  }
  
  return records;
}

std::vector<SequenceRecord> SequenceFileParser::parseFastq(const QString &filePath) {
  std::vector<SequenceRecord> records;
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return records;
  
  QTextStream in(&file);
  while (!in.atEnd()) {
    QString line1 = in.readLine().trimmed();
    if (line1.isEmpty() || !line1.startsWith('@')) continue;
    
    QString name = line1.mid(1).split(' ').first();
    if (name.isEmpty()) name = "Read";
    
    QString seq = in.readLine().trimmed().toUpper();
    QString line3 = in.readLine().trimmed(); 
    QString qStr = in.readLine().trimmed();
    
    if (seq.isEmpty()) continue;
    
    Ab1Data data;
    data.sequence = seq;
    data.isValid = true;
    
    for (int i = 0; i < qStr.length() && i < seq.length(); ++i) {
      int q = static_cast<int>(qStr[i].toLatin1()) - 33; 
      data.qualityScores.push_back(std::max(0, q));
    }
    
    for (int i = 0; i < seq.length(); ++i) {
      data.basePositions.push_back((i + 1) * 10);
    }
    
    records.push_back({name, data});
  }
  return records;
}

std::vector<SequenceRecord> SequenceFileParser::parseTxt(const QString &filePath) {
  std::vector<SequenceRecord> records;
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return records;
  
  QTextStream in(&file);
  int lineNum = 1;
  while (!in.atEnd()) {
    QString line = in.readLine().trimmed();
    if (line.isEmpty()) continue;
    
    QString seq = "";
    for (QChar c : line) {
      if (c.isLetter() || c == '-') seq += c.toUpper();
    }
    
    if (!seq.isEmpty()) {
      Ab1Data d;
      d.sequence = seq;
      d.isValid = true;
      QString name = QString("Line %1").arg(lineNum);
      records.push_back({name, d});
    }
    lineNum++;
  }
  return records;
}