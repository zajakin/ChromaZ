#include "ab1parser.h"
#include <QFile>
#include <QDataStream>
#include <QTextStream>
#include <cstring>
#include <cstdlib>
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

QString Ab1Parser::reverseComplement(const QString &seq) {
  QString res;
  res.reserve(seq.length());
  for (int i = seq.length() - 1; i >= 0; --i) {
    switch (seq[i].toUpper().toLatin1()) {
    case 'A': res += 'T'; break;
    case 'T': case 'U': res += 'A'; break;
    case 'C': res += 'G'; break;
    case 'G': res += 'C'; break;
    case 'R': res += 'Y'; break;
    case 'Y': res += 'R'; break;
    case 'K': res += 'M'; break;
    case 'M': res += 'K'; break;
    case 'B': res += 'V'; break;
    case 'V': res += 'B'; break;
    case 'D': res += 'H'; break;
    case 'H': res += 'D'; break;
    default:  res += seq[i].toUpper(); break; // S, W, N, '-' и прочее
    }
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
  sMax = std::max(sMax, (int)orig.traceA.size());
  sMax = std::max(sMax, (int)orig.traceC.size());
  sMax = std::max(sMax, (int)orig.traceG.size());
  sMax = std::max(sMax, (int)orig.traceT.size());

  res.basePositions.reserve(orig.basePositions.size());
  for (int i = (int)orig.basePositions.size() - 1; i >= 0; --i) {
    int p = orig.basePositions[i];
    int pRev = (sMax > 0) ? (sMax - 1 - p) : p;
    res.basePositions.push_back(std::max(0, pRev));
  }

  return res;
}

// ---------------------------------------------------------------------------
// SCF v2 / v3 (Staden). Заголовок 128 байт, big-endian.
// v2: сэмплы чередуются (A,C,G,T на каждый отсчёт), базы — структуры по 12 байт.
// v3: сэмплы по каналам (все A, все C, ...), с двойным delta-кодированием,
//     базы — раздельные массивы (peak[4*N], probA[N], probC[N], probG[N], probT[N], base[N]).
// ---------------------------------------------------------------------------
Ab1Data Ab1Parser::parseScf(const QString &filePath) {
  Ab1Data data;
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    data.errorMessage = "Failed to open SCF file";
    return data;
  }
  const QByteArray buf = file.readAll();
  const qint64 size = buf.size();
  const uchar *p = reinterpret_cast<const uchar *>(buf.constData());

  auto u32 = [&](qint64 off) -> uint32_t {
    if (off < 0 || off + 4 > size) return 0;
    return (uint32_t(p[off]) << 24) | (uint32_t(p[off + 1]) << 16) |
           (uint32_t(p[off + 2]) << 8) | uint32_t(p[off + 3]);
  };
  auto u16 = [&](qint64 off) -> uint32_t {
    if (off < 0 || off + 2 > size) return 0;
    return (uint32_t(p[off]) << 8) | uint32_t(p[off + 1]);
  };
  auto u8 = [&](qint64 off) -> uint32_t {
    if (off < 0 || off >= size) return 0;
    return p[off];
  };

  if (size < 128 || u32(0) != 0x2e736366) { // ".scf"
    data.errorMessage = "Invalid SCF format";
    return data;
  }

  const qint64 numSamples = u32(4);
  const qint64 samplesOff = u32(8);
  const qint64 numBases   = u32(12);
  const qint64 basesOff   = u32(24);
  const QByteArray ver    = buf.mid(36, 4);
  const int sampleSize    = (int)u32(40);
  const bool isV3         = std::atof(ver.constData()) >= 3.0;

  if (sampleSize != 1 && sampleSize != 2) { data.errorMessage = "Unsupported SCF sample size"; return data; }
  if (numSamples <= 0 || numSamples > 50000000 || numBases < 0 || numBases > 10000000) {
    data.errorMessage = "Corrupt SCF header";
    return data;
  }
  if (samplesOff + numSamples * 4 * sampleSize > size) { data.errorMessage = "Truncated SCF samples"; return data; }
  const qint64 baseBytes = isV3 ? numBases * 18 : numBases * 12;
  if (basesOff + baseBytes > size) { data.errorMessage = "Truncated SCF bases"; return data; }

  std::vector<int> *traces[4] = {&data.traceA, &data.traceC, &data.traceG, &data.traceT};
  for (auto *t : traces) t->assign(numSamples, 0);

  const uint32_t mask = (sampleSize == 1) ? 0xFFu : 0xFFFFu;
  auto readSample = [&](qint64 off) { return sampleSize == 1 ? u8(off) : u16(off); };

  if (isV3) {
    for (int ch = 0; ch < 4; ++ch) {
      std::vector<uint32_t> raw(numSamples);
      qint64 base = samplesOff + qint64(ch) * numSamples * sampleSize;
      for (qint64 i = 0; i < numSamples; ++i) raw[i] = readSample(base + i * sampleSize);
      for (int pass = 0; pass < 2; ++pass) { // двойное delta-декодирование
        uint32_t prev = 0;
        for (qint64 i = 0; i < numSamples; ++i) { prev = (prev + raw[i]) & mask; raw[i] = prev; }
      }
      for (qint64 i = 0; i < numSamples; ++i) (*traces[ch])[i] = (int)raw[i];
    }
  } else {
    for (qint64 i = 0; i < numSamples; ++i)
      for (int ch = 0; ch < 4; ++ch)
        (*traces[ch])[i] = (int)readSample(samplesOff + (i * 4 + ch) * sampleSize);
  }

  QString seq;
  seq.reserve((int)numBases);
  for (qint64 i = 0; i < numBases; ++i) {
    uint32_t peak, pr[4], chr;
    if (isV3) {
      peak = u32(basesOff + i * 4);
      for (int c = 0; c < 4; ++c) pr[c] = u8(basesOff + numBases * 4 + qint64(c) * numBases + i);
      chr = u8(basesOff + numBases * 8 + i);
    } else {
      qint64 o = basesOff + i * 12;
      peak = u32(o);
      for (int c = 0; c < 4; ++c) pr[c] = u8(o + 4 + c);
      chr = u8(o + 8);
    }
    QChar bc = QChar(char(chr)).toUpper();
    seq += bc;
    data.basePositions.push_back((int)peak);
    int q = 0;
    switch (bc.toLatin1()) {
    case 'A': q = pr[0]; break;
    case 'C': q = pr[1]; break;
    case 'G': q = pr[2]; break;
    case 'T': q = pr[3]; break;
    default:  q = 0; break;
    }
    data.qualityScores.push_back(q);
  }
  data.sequence = seq;
  data.isValid = !data.sequence.isEmpty();
  if (!data.isValid) data.errorMessage = "SCF contains no bases";
  return data;
}

// ---------------------------------------------------------------------------
// ABIF (.ab1)
// ---------------------------------------------------------------------------
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

  char magic[4] = {0, 0, 0, 0};
  in.readRawData(magic, 4);
  if (std::memcmp(magic, "ABIF", 4) != 0) {
    file.close();
    return parseScf(filePath);
  }

  uint16_t version;
  in >> version;

  DirEntry rootDir;
  in.readRawData(rootDir.tag, 4);
  in >> rootDir.number >> rootDir.elemType >> rootDir.elemSize
     >> rootDir.numElems >> rootDir.dataSize >> rootDir.dataOffset;

  if (in.status() != QDataStream::Ok || rootDir.numElems == 0 || rootDir.numElems > 100000 ||
      (qint64)rootDir.dataOffset >= file.size()) {
    data.errorMessage = "Corrupt ABIF directory";
    return data;
  }

  file.seek(rootDir.dataOffset);

  std::vector<DirEntry> entries;
  for (uint32_t i = 0; i < rootDir.numElems; ++i) {
    DirEntry e;
    in.readRawData(e.tag, 4);
    in >> e.number >> e.elemType >> e.elemSize
       >> e.numElems >> e.dataSize >> e.dataOffset;
    uint32_t handle; in >> handle;
    if (in.status() != QDataStream::Ok) break;
    entries.push_back(e);
  }

  auto hasEntry = [&](const char tag[4], uint32_t num) {
    for (const auto &e : entries)
      if (std::memcmp(e.tag, tag, 4) == 0 && e.number == num) return true;
    return false;
  };
  // Предпочитаем отредактированные данные (номер 2), иначе оригинальные (номер 1).
  auto preferred = [&](const char tag[4]) -> uint32_t { return hasEntry(tag, 2) ? 2u : 1u; };

  auto readShortArray = [&](const char tag[4], uint32_t num, bool isSigned) -> std::vector<int> {
    std::vector<int> result;
    for (const auto &e : entries) {
      if (std::memcmp(e.tag, tag, 4) == 0 && e.number == num) {
        if ((qint64)e.dataOffset + (qint64)e.numElems * 2 > file.size()) break;
        file.seek(e.dataOffset);
        result.reserve(e.numElems);
        for (uint32_t k = 0; k < e.numElems; ++k) {
          if (isSigned) { int16_t v; in >> v; result.push_back(std::max<int>(0, v)); }
          else          { uint16_t v; in >> v; result.push_back(v); }
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

  std::vector<int> d9  = readShortArray("DATA", 9,  true);
  std::vector<int> d10 = readShortArray("DATA", 10, true);
  std::vector<int> d11 = readShortArray("DATA", 11, true);
  std::vector<int> d12 = readShortArray("DATA", 12, true);

  std::vector<std::vector<int>*> traces = {&d9, &d10, &d11, &d12};
  for (int i = 0; i < 4 && i < fwo.length(); ++i) {
    QChar base = fwo[i].toUpper();
    if (base == 'A') data.traceA = *traces[i];
    else if (base == 'C') data.traceC = *traces[i];
    else if (base == 'G') data.traceG = *traces[i];
    else if (base == 'T') data.traceT = *traces[i];
  }

  const uint32_t pbasNum = preferred("PBAS");
  for (const auto &e : entries) {
    if (std::memcmp(e.tag, "PBAS", 4) == 0 && e.number == pbasNum) {
      if (e.dataSize <= 4) { // очень короткие данные хранятся прямо в поле смещения
        char chars[4] = {char((e.dataOffset >> 24) & 0xFF), char((e.dataOffset >> 16) & 0xFF),
                         char((e.dataOffset >> 8) & 0xFF), char(e.dataOffset & 0xFF)};
        data.sequence = QString::fromLatin1(chars, (int)std::min<uint32_t>(e.numElems, 4));
      } else {
        file.seek(e.dataOffset);
        data.sequence = QString::fromLatin1(file.read(e.numElems));
      }
      break;
    }
  }
  data.sequence = data.sequence.toUpper();

  // PLOC и PCON берём с тем же номером, что и PBAS, чтобы индексы совпадали.
  data.basePositions = readShortArray("PLOC", pbasNum, false);
  if (data.basePositions.empty()) data.basePositions = readShortArray("PLOC", pbasNum == 2 ? 1 : 2, false);

  for (const auto &e : entries) {
    if (std::memcmp(e.tag, "PCON", 4) == 0 && e.number == pbasNum) {
      file.seek(e.dataOffset);
      QByteArray qData = file.read(e.numElems);
      for (char b : qData) data.qualityScores.push_back(static_cast<uint8_t>(b));
      break;
    }
  }
  if (data.qualityScores.empty()) {
    for (const auto &e : entries) {
      if (std::memcmp(e.tag, "PCON", 4) == 0) {
        file.seek(e.dataOffset);
        QByteArray qData = file.read(e.numElems);
        for (char b : qData) data.qualityScores.push_back(static_cast<uint8_t>(b));
        break;
      }
    }
  }

  data.isValid = !data.sequence.isEmpty();
  if (!data.isValid) data.errorMessage = "No sequence found in AB1 file";
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
  QString currentName;
  QString currentSeq;
  bool isGenBank = false; // внутри записи GenBank (после LOCUS)
  bool inOrigin = false;  // внутри секции ORIGIN — только там лежит последовательность

  auto flush = [&]() {
    if (!currentSeq.isEmpty()) {
      Ab1Data d; d.sequence = currentSeq; d.isValid = true;
      records.push_back({currentName.isEmpty() ? QString("Sequence") : currentName, d});
    }
    currentSeq.clear();
  };
  auto addLetters = [&](const QString &line) {
    for (QChar c : line) {
      if (c.isLetter() || c == '-') currentSeq += c.toUpper();
    }
  };

  while (!in.atEnd()) {
    QString line = in.readLine().trimmed();
    if (line.isEmpty()) continue;

    if (line.startsWith('>')) {
      flush();
      isGenBank = false; inOrigin = false;
      currentName = line.mid(1).trimmed().split(' ').first();
    } else if (line.startsWith("LOCUS")) {
      flush();
      isGenBank = true; inOrigin = false;
      QStringList parts = line.split(' ', Qt::SkipEmptyParts);
      currentName = parts.size() > 1 ? parts[1] : QString();
    } else if (isGenBank) {
      // Заголовки, FEATURES и пр. НЕ являются последовательностью.
      if (line.startsWith("ORIGIN")) inOrigin = true;
      else if (line.startsWith("//")) { flush(); currentName.clear(); isGenBank = false; inOrigin = false; }
      else if (inOrigin) addLetters(line);
    } else {
      if (line.startsWith("//")) { flush(); currentName.clear(); }
      else addLetters(line);
    }
  }
  flush();
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
    in.readLine(); // '+'
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
    if (line.isEmpty()) { lineNum++; continue; }

    QString seq;
    for (QChar c : line) {
      if (c.isLetter() || c == '-') seq += c.toUpper();
    }
    if (!seq.isEmpty()) {
      Ab1Data d;
      d.sequence = seq;
      d.isValid = true;
      records.push_back({QString("Line %1").arg(lineNum), d});
    }
    lineNum++;
  }
  return records;
}
