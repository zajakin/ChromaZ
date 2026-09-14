#include "contigwidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QClipboard>
#include <QMenu>
#include <QFile>
#include <QTextStream>
#include <QMimeData>
#include <QScrollArea>
#include <QScrollBar>
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <map>
#include <algorithm>

namespace {
struct CursorGuard {
  CursorGuard() { QGuiApplication::setOverrideCursor(Qt::WaitCursor); }
  ~CursorGuard() { QGuiApplication::restoreOverrideCursor(); }
};
}

ContigWidget::ContigWidget(QWidget *parent) : QWidget(parent) {
  QPalette p = palette();
  p.setColor(QPalette::Base, Qt::white);
  p.setColor(QPalette::Window, Qt::white);
  p.setColor(QPalette::Text, Qt::black);
  p.setColor(QPalette::WindowText, Qt::black);
  setPalette(p);
  
  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setAcceptDrops(true);
  updateLayoutGeometry();
}

void ContigWidget::pushUndoState() {
  WidgetState state;
  state.referenceSeq = referenceSeq;
  state.consensusSeq = consensusSeq;
  state.tracks = tracks;
  state.selTrack = selTrack;
  state.selStartCol = selStartCol;
  state.selEndCol = selEndCol;
  state.autoRCEnabled = autoRCEnabled;
  
  undoStack.push_back(state);
  if (undoStack.size() > 100) {
    undoStack.erase(undoStack.begin());
  }
  redoStack.clear();
}

void ContigWidget::undo() {
  if (undoStack.empty()) return;
  
  WidgetState currentState;
  currentState.referenceSeq = referenceSeq;
  currentState.consensusSeq = consensusSeq;
  currentState.tracks = tracks;
  currentState.selTrack = selTrack;
  currentState.selStartCol = selStartCol;
  currentState.selEndCol = selEndCol;
  currentState.autoRCEnabled = autoRCEnabled;
  redoStack.push_back(currentState);
  
  WidgetState prevState = undoStack.back();
  undoStack.pop_back();
  
  referenceSeq = prevState.referenceSeq;
  consensusSeq = prevState.consensusSeq;
  tracks = prevState.tracks;
  selTrack = prevState.selTrack;
  selStartCol = prevState.selStartCol;
  selEndCol = prevState.selEndCol;
  autoRCEnabled = prevState.autoRCEnabled;
  emit autoRCChanged(autoRCEnabled);
  
  calculateConsensusAndSNPs();
  updateLayoutGeometry();
  fitToWindowHeight();
  update();
}

void ContigWidget::redo() {
  if (redoStack.empty()) return;
  
  WidgetState currentState;
  currentState.referenceSeq = referenceSeq;
  currentState.consensusSeq = consensusSeq;
  currentState.tracks = tracks;
  currentState.selTrack = selTrack;
  currentState.selStartCol = selStartCol;
  currentState.selEndCol = selEndCol;
  currentState.autoRCEnabled = autoRCEnabled;
  undoStack.push_back(currentState);
  
  WidgetState nextState = redoStack.back();
  redoStack.pop_back();
  
  referenceSeq = nextState.referenceSeq;
  consensusSeq = nextState.consensusSeq;
  tracks = nextState.tracks;
  selTrack = nextState.selTrack;
  selStartCol = nextState.selStartCol;
  selEndCol = nextState.selEndCol;
  autoRCEnabled = nextState.autoRCEnabled;
  emit autoRCChanged(autoRCEnabled);
  
  calculateConsensusAndSNPs();
  updateLayoutGeometry();
  fitToWindowHeight();
  update();
}

QChar ContigWidget::translateCodon(const QString& codon, int tableIndex) const {
  if (codon.length() != 3 || codon.contains('-') || codon.contains('N')) return ' ';
  QString c = codon.toUpper();
  if (c=="TTT" || c=="TTC") return 'F';
  if (c=="TTA" || c=="TTG") return 'L'; 
  if (c=="CTT" || c=="CTC" || c=="CTA" || c=="CTG") return 'L';
  if (c=="ATT" || c=="ATC") return 'I';
  if (c=="ATA") return (tableIndex == 1) ? 'M' : 'I';
  if (c=="ATG") return 'M';
  if (c=="GTT" || c=="GTC" || c=="GTA" || c=="GTG") return 'V';
  if (c=="TCT" || c=="TCC" || c=="TCA" || c=="TCG") return 'S';
  if (c=="CCT" || c=="CCC" || c=="CCA" || c=="CCG") return 'P';
  if (c=="ACT" || c=="ACC" || c=="ACA" || c=="ACG") return 'T';
  if (c=="GCT" || c=="GCC" || c=="GCA" || c=="GCG") return 'A';
  if (c=="TAT" || c=="TAC") return 'Y';
  if (c=="TAA" || c=="TAG") return '*';
  if (c=="CAT" || c=="CAC") return 'H';
  if (c=="CAA" || c=="CAG") return 'Q';
  if (c=="AAT" || c=="AAC") return 'N';
  if (c=="AAA" || c=="AAG") return 'K';
  if (c=="GAT" || c=="GAC") return 'D';
  if (c=="GAA" || c=="GAG") return 'E';
  if (c=="TGT" || c=="TGC") return 'C';
  if (c=="TGA") return (tableIndex == 1) ? 'W' : '*';
  if (c=="TGG") return 'W';
  if (c=="CGT" || c=="CGC" || c=="CGA" || c=="CGG") return 'R';
  if (c=="AGT" || c=="AGC") return 'S';
  if (c=="AGA" || c=="AGG") return (tableIndex == 1) ? '*' : 'R';
  if (c=="GGT" || c=="GGC" || c=="GGA" || c=="GGG") return 'G';
  return ' ';
}

// Генерация 6 рамок считывания (+1, +2, +3, -1, -2, -3)
QString ContigWidget::getOrfLine(int frame) const {
  bool isReverse = (frame >= 3);
  int shift = frame % 3;
  
  QString workingRef = isReverse ? Ab1Parser::reverseComplement(referenceSeq) : referenceSeq;
  QString result(referenceSeq.length(), ' ');
  
  std::vector<int> refMap;
  QString ungappedRef = "";
  for (int col = 0; col < workingRef.length(); ++col) {
    if (workingRef[col] != '-') {
      ungappedRef += workingRef[col];
      refMap.push_back(col);
    }
  }
  
  int n = ungappedRef.length();
  for (int i = shift; i + 2 < n; i += 3) {
    QString codon = ungappedRef.mid(i, 3);
    QChar aa = translateCodon(codon, translationTable);
    if (aa != ' ') {
      int centerCol = refMap[i + 1];
      if (isReverse) centerCol = referenceSeq.length() - 1 - centerCol;
      if (centerCol >= 0 && centerCol < result.length()) {
        result[centerCol] = aa;
      }
    }
  }
  return result;
}

int ContigWidget::getScrollX() const {
  const QWidget *w = this;
  while (w) {
    const QScrollArea *sa = qobject_cast<const QScrollArea*>(w);
    if (sa && sa->horizontalScrollBar()) return sa->horizontalScrollBar()->value();
    w = w->parentWidget();
  }
  return 0;
}

int ContigWidget::getNumInContig() const {
  int count = 0;
  for (const auto &tr : tracks) if (tr.inContig) count++;
  return count;
}

int ContigWidget::getTrackY(int trackIdx) const {
  int numIn = getNumInContig();
  int curY = trackStartY;
  for (int t = 0; t <= trackIdx && t < (int)tracks.size(); ++t) {
    if (t == numIn && numIn < (int)tracks.size()) curY += 35;
    if (t == trackIdx) return curY;
    curY += trackHeight;
  }
  return curY;
}

bool ContigWidget::isFileLoaded(const QString &filePath) const {
  for (const auto &track : tracks) if (track.filePath == filePath) return true;
  return false;
}

bool ContigWidget::addAb1Track(const QString &filePath, const Ab1Data &data) {
  if (isFileLoaded(filePath)) return false;
  pushUndoState();
  AlignedTrack track;
  track.filePath = filePath;
  track.fileName = QFileInfo(filePath).fileName();
  track.originalData = data;
  tracks.push_back(track);
  fitToWindowHeight();
  updateLayoutGeometry();
  update();
  return true;
}

void ContigWidget::setAutoRC(bool enable) {
  if (autoRCEnabled == enable) return;
  pushUndoState();
  autoRCEnabled = enable;
  runAlignment();
}

void ContigWidget::setAlignmentAlgorithm(int algoIdx) {
  if (algoIdx >= 0 && algoIdx <= 7) {
    currentAlgorithm = static_cast<AlignmentAlgorithm>(algoIdx);
    if (isAutoReference) {
      referenceSeq.clear();
      consensusSeq.clear();
      isAutoReference = false;
      for (auto &tr : tracks) {
        tr.alignment = AlignmentResult();
      }
    }
    runAlignment();
  }
}

void ContigWidget::setTranslationTable(int tableIdx) {
  translationTable = tableIdx;
  update();
}

void ContigWidget::setMinIdentityThreshold(double val) {
  pushUndoState();
  minIdentityThreshold = val;
  for (auto &track : tracks) {
    track.inContig = (track.alignment.identity >= minIdentityThreshold);
  }
  std::stable_sort(tracks.begin(), tracks.end(), [](const AlignedTrack &a, const AlignedTrack &b) {
    return a.inContig > b.inContig;
  });
  calculateConsensusAndSNPs();
  fitToWindowHeight();
  updateLayoutGeometry();
  update();
}

void ContigWidget::setConsensusIupac(bool enable) {
  useIupacConsensus = enable;
  calculateConsensusAndSNPs();
  update();
}

bool ContigWidget::saveProject(const QString &filePath) {
  QJsonObject root;
  root["version"] = "1.0";
  root["format"] = "ChromaZ";
  root["referenceSeq"] = referenceSeq;
  root["consensusSeq"] = consensusSeq;
  root["isAutoReference"] = isAutoReference;
  root["minIdentityThreshold"] = minIdentityThreshold;
  root["useIupacConsensus"] = useIupacConsensus;
  root["translationTable"] = translationTable;
  root["currentAlgorithm"] = static_cast<int>(currentAlgorithm);
  root["autoRCEnabled"] = autoRCEnabled;
  root["scaleX"] = scaleX;
  root["scaleY"] = scaleY;
  root["trackHeight"] = trackHeight;
  root["leftMarginWidth"] = leftMarginWidth;
  
  QJsonArray tracksArray;
  for (const auto &tr : tracks) {
    QJsonObject trObj;
    trObj["filePath"] = tr.filePath;
    trObj["fileName"] = tr.fileName;
    trObj["inContig"] = tr.inContig;
    trObj["overrideRC"] = tr.overrideRC;
    
    QJsonObject ab1Obj;
    ab1Obj["sequence"] = tr.originalData.sequence;
    ab1Obj["isValid"] = tr.originalData.isValid;
    
    auto vecToJson = [](const std::vector<int> &vec) {
      QJsonArray arr;
      for (int v : vec) arr.append(v);
      return arr;
    };
    
    ab1Obj["traceA"] = vecToJson(tr.originalData.traceA);
    ab1Obj["traceC"] = vecToJson(tr.originalData.traceC);
    ab1Obj["traceG"] = vecToJson(tr.originalData.traceG);
    ab1Obj["traceT"] = vecToJson(tr.originalData.traceT);
    ab1Obj["basePositions"] = vecToJson(tr.originalData.basePositions);
    ab1Obj["qualityScores"] = vecToJson(tr.originalData.qualityScores);
    trObj["originalData"] = ab1Obj;
    
    QJsonObject algObj;
    algObj["alignedRef"] = tr.alignment.alignedRef;
    algObj["alignedRead"] = tr.alignment.alignedRead;
    algObj["score"] = tr.alignment.score;
    algObj["identity"] = tr.alignment.identity;
    algObj["isReverseComplement"] = tr.alignment.isReverseComplement;
    trObj["alignment"] = algObj;
    
    tracksArray.append(trObj);
  }
  root["tracks"] = tracksArray;
  
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) return false;
  
  file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  return true;
}

bool ContigWidget::loadProject(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) return false;
  
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isObject()) return false;
  
  clearAll();
  QJsonObject root = doc.object();
  
  referenceSeq = root["referenceSeq"].toString();
  consensusSeq = root["consensusSeq"].toString();
  isAutoReference = root["isAutoReference"].toBool();
  minIdentityThreshold = root["minIdentityThreshold"].toDouble(45.0);
  useIupacConsensus = root["useIupacConsensus"].toBool(false);
  translationTable = root["translationTable"].toInt(0);
  currentAlgorithm = static_cast<AlignmentAlgorithm>(root["currentAlgorithm"].toInt(0));
  autoRCEnabled = root["autoRCEnabled"].toBool(true);
  scaleX = root["scaleX"].toDouble(14.0);
  scaleY = root["scaleY"].toDouble(0.05);
  trackHeight = root["trackHeight"].toInt(120);
  leftMarginWidth = root["leftMarginWidth"].toInt(220);
  
  auto jsonToVec = [](const QJsonArray &arr) {
    std::vector<int> vec;
    for (auto val : arr) vec.push_back(val.toInt());
    return vec;
  };
  
  QJsonArray tracksArray = root["tracks"].toArray();
  for (auto val : tracksArray) {
    QJsonObject trObj = val.toObject();
    AlignedTrack tr;
    tr.filePath = trObj["filePath"].toString();
    tr.fileName = trObj["fileName"].toString();
    tr.inContig = trObj["inContig"].toBool();
    tr.overrideRC = trObj["overrideRC"].toBool(false);
    
    QJsonObject ab1Obj = trObj["originalData"].toObject();
    tr.originalData.sequence = ab1Obj["sequence"].toString();
    tr.originalData.isValid = ab1Obj["isValid"].toBool();
    tr.originalData.traceA = jsonToVec(ab1Obj["traceA"].toArray());
    tr.originalData.traceC = jsonToVec(ab1Obj["traceC"].toArray());
    tr.originalData.traceG = jsonToVec(ab1Obj["traceG"].toArray());
    tr.originalData.traceT = jsonToVec(ab1Obj["traceT"].toArray());
    tr.originalData.basePositions = jsonToVec(ab1Obj["basePositions"].toArray());
    tr.originalData.qualityScores = jsonToVec(ab1Obj["qualityScores"].toArray());
    
    QJsonObject algObj = trObj["alignment"].toObject();
    tr.alignment.alignedRef = algObj["alignedRef"].toString();
    tr.alignment.alignedRead = algObj["alignedRead"].toString();
    tr.alignment.score = algObj["score"].toInt();
    tr.alignment.identity = algObj["identity"].toDouble();
    tr.alignment.isReverseComplement = algObj["isReverseComplement"].toBool();
    
    tracks.push_back(tr);
  }
  
  calculateConsensusAndSNPs();
  fitToWindowHeight();
  updateLayoutGeometry();
  update();
  undoStack.clear();
  redoStack.clear();
  emit autoRCChanged(autoRCEnabled);
  emit projectLoaded(filePath);
  return true;
}

void ContigWidget::trimLowQualityEnds(int minPhred) {
  pushUndoState();
  for (auto &tr : tracks) {
    auto &q = tr.originalData.qualityScores;
    auto &seq = tr.originalData.sequence;
    auto &ploc = tr.originalData.basePositions;
    
    if (seq.isEmpty()) continue;
    
    int n = seq.length();
    int qLen = q.size();
    int pLen = ploc.size();
    
    int start = 0;
    while (start < n && start < qLen && q[start] < minPhred) start++;
    
    int end = std::min(n, qLen) - 1;
    while (end >= start && q[end] < minPhred) end--;
    
    if (start <= end && start < n) {
      int newLen = end - start + 1;
      seq = seq.mid(start, newLen);
      
      if (start < qLen) {
        int newQLen = std::min(newLen, qLen - start);
        q = std::vector<int>(q.begin() + start, q.begin() + start + newQLen);
      } else {
        q.clear();
      }
      
      if (start < pLen) {
        int newPLen = std::min(newLen, pLen - start);
        ploc = std::vector<int>(ploc.begin() + start, ploc.begin() + start + newPLen);
      } else {
        ploc.clear();
      }
    } else {
      seq.clear();
      q.clear();
      ploc.clear();
    }
  }
  runAlignment();
}

void ContigWidget::findSequence(const QString &query, bool forward) {
  QString qClean = query.trimmed().toUpper();
  if (qClean.isEmpty()) {
    searchMatches.clear();
    currentMatchIdx = -1;
    lastSearchQuery.clear();
    selTrack = SEL_NONE;
    update();
    return;
  }
  
  if (qClean != lastSearchQuery || searchMatches.empty()) {
    lastSearchQuery = qClean;
    searchMatches.clear();
    currentMatchIdx = -1;
    int qLen = qClean.length();
    
    int pos = 0;
    while ((pos = referenceSeq.toUpper().indexOf(qClean, pos)) != -1) {
      searchMatches.push_back({SEL_REF, pos, qLen});
      pos++;
    }
    
    pos = 0;
    while ((pos = consensusSeq.toUpper().indexOf(qClean, pos)) != -1) {
      searchMatches.push_back({SEL_CONSENSUS, pos, qLen});
      pos++;
    }
    
    for (size_t t = 0; t < tracks.size(); ++t) {
      QString read = tracks[t].alignment.alignedRead.isEmpty() ? 
      tracks[t].originalData.sequence : tracks[t].alignment.alignedRead;
      pos = 0;
      while ((pos = read.toUpper().indexOf(qClean, pos)) != -1) {
        searchMatches.push_back({(int)t, pos, qLen});
        pos++;
      }
    }
  }
  
  if (searchMatches.empty()) {
    currentMatchIdx = -1;
    selTrack = SEL_NONE;
    update();
    return;
  }
  
  if (forward) {
    currentMatchIdx = (currentMatchIdx + 1) % searchMatches.size();
  } else {
    currentMatchIdx = (currentMatchIdx - 1 + searchMatches.size()) % searchMatches.size();
  }
  
  auto match = searchMatches[currentMatchIdx];
  selTrack = match.trackId;
  selStartCol = match.startCol;
  selEndCol = match.startCol + match.length - 1;
  
  const QWidget *w = this;
  while (w) {
    const QScrollArea *sa = qobject_cast<const QScrollArea*>(w);
    if (sa) {
      if (sa->horizontalScrollBar()) {
        int targetX = leftMarginWidth + match.startCol * scaleX;
        sa->horizontalScrollBar()->setValue(targetX - sa->viewport()->width() / 2);
      }
      if (sa->verticalScrollBar()) {
        int targetY = 0;
        if (match.trackId == SEL_REF) targetY = 10;
        else if (match.trackId == SEL_CONSENSUS) targetY = 80;
        else if (match.trackId >= 0 && match.trackId < (int)tracks.size()) {
          targetY = getTrackY(match.trackId);
        }
        sa->verticalScrollBar()->setValue(targetY - sa->viewport()->height() / 2);
      }
      break;
    }
    w = w->parentWidget();
  }
  
  update();
}

void ContigWidget::calculateConsensusAndSNPs() {
  consensusSeq.clear();
  snpColumns.clear();
  int maxCols = getMaxCols();
  
  for (int col = 0; col < maxCols; ++col) {
    int countA = 0, countC = 0, countG = 0, countT = 0, active = 0;
    std::map<QChar, int> baseFreq;
    
    for (const auto &tr : tracks) {
      if (!tr.inContig) continue;
      QString read = tr.alignment.alignedRead.isEmpty() ? tr.originalData.sequence : tr.alignment.alignedRead;
      if (col < read.length()) {
        QChar b = read[col].toUpper();
        if (b != '-' && b != 'N') {
          active++;
          if (b == 'A') countA++;
          else if (b == 'C') countC++;
          else if (b == 'G') countG++;
          else if (b == 'T') countT++;
          baseFreq[b]++;
        }
      }
    }
    
    if (active == 0) {
      consensusSeq += '-';
      continue;
    }
    
    int maxCount = std::max({countA, countC, countG, countT});
    int topCount = (countA == maxCount) + (countC == maxCount) + (countG == maxCount) + (countT == maxCount);
    
    if (!useIupacConsensus) {
      if (topCount == 1) {
        if (countA == maxCount) consensusSeq += 'A';
        else if (countC == maxCount) consensusSeq += 'C';
        else if (countG == maxCount) consensusSeq += 'G';
        else if (countT == maxCount) consensusSeq += 'T';
      } else {
        consensusSeq += 'N';
      }
    } else {
      if (topCount == 1) {
        if (countA == maxCount) consensusSeq += 'A';
        else if (countC == maxCount) consensusSeq += 'C';
        else if (countG == maxCount) consensusSeq += 'G';
        else if (countT == maxCount) consensusSeq += 'T';
      } else {
        if (countA > 0 && countG > 0 && countC == 0 && countT == 0) consensusSeq += 'R';
        else if (countC > 0 && countT > 0 && countA == 0 && countG == 0) consensusSeq += 'Y';
        else if (countG > 0 && countC > 0 && countA == 0 && countT == 0) consensusSeq += 'S';
        else if (countA > 0 && countT > 0 && countC == 0 && countG == 0) consensusSeq += 'W';
        else if (countG > 0 && countT > 0 && countA == 0 && countC == 0) consensusSeq += 'K';
        else if (countA > 0 && countC > 0 && countG == 0 && countT == 0) consensusSeq += 'M';
        else consensusSeq += 'N';
      }
    }
    
    if (active >= 2 && baseFreq.size() > 1) {
      snpColumns.push_back(col);
    }
  }
}

void ContigWidget::jumpNextSNP() {
  if (snpColumns.empty()) return;
  currentSnpIdx = (currentSnpIdx + 1) % snpColumns.size();
  int col = snpColumns[currentSnpIdx];
  
  const QWidget *w = this;
  while (w) {
    const QScrollArea *sa = qobject_cast<const QScrollArea*>(w);
    if (sa && sa->horizontalScrollBar()) {
      sa->horizontalScrollBar()->setValue(leftMarginWidth + col * scaleX - sa->viewport()->width() / 2);
      break;
    }
    w = w->parentWidget();
  }
  update();
}

void ContigWidget::jumpPrevSNP() {
  if (snpColumns.empty()) return;
  currentSnpIdx = (currentSnpIdx - 1 + snpColumns.size()) % snpColumns.size();
  int col = snpColumns[currentSnpIdx];
  
  const QWidget *w = this;
  while (w) {
    const QScrollArea *sa = qobject_cast<const QScrollArea*>(w);
    if (sa && sa->horizontalScrollBar()) {
      sa->horizontalScrollBar()->setValue(leftMarginWidth + col * scaleX - sa->viewport()->width() / 2);
      break;
    }
    w = w->parentWidget();
  }
  update();
}

void ContigWidget::exportConsensusFasta(const QString &filePath) {
  QFile file(filePath);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream out(&file);
    out << ">ChromaZ_Consensus\n" << consensusSeq << "\n";
  }
}

void ContigWidget::exportViewImage(const QString &filePath) {
  QImage img(size(), QImage::Format_ARGB32);
  img.fill(Qt::white);
  QPainter p(&img);
  render(&p);
  img.save(filePath);
}

void ContigWidget::exportPdf(const QString &filePath, int basesPerLine) {
  if (basesPerLine <= 0) basesPerLine = 80;
  int maxCols = getMaxCols();
  if (maxCols == 0) return;
  
  QPdfWriter writer(filePath);
  writer.setPageSize(QPageSize(QPageSize::A4));
  writer.setPageOrientation(QPageLayout::Portrait);
  writer.setResolution(300);
  
  QPainter painter(&writer);
  painter.setRenderHint(QPainter::Antialiasing);
  
  QRect pageRect = writer.pageLayout().paintRectPixels(writer.resolution());
  int pageWidth = pageRect.width();
  int pageHeight = pageRect.height();
  
  int pdfLeftMargin = 350;
  double pdfScaleX = double(pageWidth - pdfLeftMargin - 100) / basesPerLine;
  pdfScaleX = std::max(8.0, pdfScaleX);
  
  int pdfTrackHeight = 140;
  int pdfBlockHeight = 140 + (tracks.size() + 2) * pdfTrackHeight;
  
  int currentY = 50;
  int totalBlocks = (maxCols + basesPerLine - 1) / basesPerLine;
  
  QFont seqFont("Monospace", 7, QFont::Bold);
  QFont labelFont("SansSerif", 8);
  QFont headerFont("SansSerif", 9, QFont::Bold);
  
  for (int b = 0; b < totalBlocks; ++b) {
    int startCol = b * basesPerLine;
    int endCol = std::min(maxCols, (b + 1) * basesPerLine);
    
    if (currentY + pdfBlockHeight > pageHeight - 50 && currentY > 50) {
      writer.newPage();
      currentY = 50;
    }
    
    painter.setFont(headerFont);
    painter.setPen(Qt::black);
    painter.drawText(50, currentY + 15, QString("Block %1 (Bases %2 - %3)").arg(b + 1).arg(startCol + 1).arg(endCol));
    currentY += 25;
    
    painter.setFont(headerFont);
    painter.setPen(Qt::darkGreen);
    painter.drawText(50, currentY + 18, isAutoReference ? "CONTIG:" : "REF:");
    
    painter.setFont(seqFont);
    for (int col = startCol; col < endCol && col < referenceSeq.length(); ++col) {
      int x = pdfLeftMargin + (col - startCol) * pdfScaleX;
      QChar base = referenceSeq[col].toUpper();
      if (base == 'A') painter.setPen(Qt::green);
      else if (base == 'C') painter.setPen(Qt::blue);
      else if (base == 'G') painter.setPen(Qt::black);
      else if (base == 'T') painter.setPen(Qt::red);
      else painter.setPen(Qt::gray);
      painter.drawText(x, currentY + 18, QString(base));
    }
    currentY += 25;
    
    currentY += 80; // Место для 6 рамок ORF
    
    painter.setFont(headerFont);
    painter.setPen(QColor(156, 39, 176));
    painter.drawText(50, currentY + 18, "CONSENSUS:");
    
    painter.setFont(seqFont);
    for (int col = startCol; col < endCol && col < consensusSeq.length(); ++col) {
      int x = pdfLeftMargin + (col - startCol) * pdfScaleX;
      QChar base = consensusSeq[col].toUpper();
      if (base == 'A') painter.setPen(Qt::green);
      else if (base == 'C') painter.setPen(Qt::blue);
      else if (base == 'G') painter.setPen(Qt::black);
      else if (base == 'T') painter.setPen(Qt::red);
      else if (base == '-') painter.setPen(Qt::gray);
      else painter.setPen(QColor(156, 39, 176));
      painter.drawText(x, currentY + 18, QString(base));
    }
    currentY += 25;
    
    for (size_t t = 0; t < tracks.size(); ++t) {
      auto &track = tracks[t];
      int trackY = currentY;
      
      Ab1Data dispData = Ab1Parser::getOrientedData(track.originalData, track.alignment.isReverseComplement);
      
      painter.setFont(labelFont);
      painter.setPen(track.inContig ? Qt::darkBlue : Qt::red);
      QRect nameRect(50, trackY, pdfLeftMargin - 60, pdfTrackHeight - 5);
      QString rcMark = track.alignment.isReverseComplement ? " [RC]" : "";
      painter.drawText(nameRect, Qt::TextWordWrap | Qt::TextWrapAnywhere, track.fileName + rcMark);
      
      QString readAligned = track.alignment.alignedRead.isEmpty() ? 
      dispData.sequence : track.alignment.alignedRead;
      
      struct BasePoint { double x; int col; };
      std::vector<BasePoint> basePoints;
      for (int col = startCol; col < endCol && col < readAligned.length(); ++col) {
        if (readAligned[col] != '-') basePoints.push_back({pdfLeftMargin + (col - startCol) * pdfScaleX, col});
      }
      
      int rawBaseIdx = 0;
      for (int col = 0; col < startCol && col < readAligned.length(); ++col) {
        if (readAligned[col] != '-') rawBaseIdx++;
      }
      
      painter.setFont(seqFont);
      for (int col = startCol; col < endCol && col < readAligned.length(); ++col) {
        int x = pdfLeftMargin + (col - startCol) * pdfScaleX;
        QChar base = readAligned[col];
        QChar refC = (col < referenceSeq.length()) ? referenceSeq[col].toUpper() : '?';
        
        bool isMismatch = (base != '-' && refC != '?' && refC != '-' && base.toUpper() != refC);
        if (isMismatch) {
          painter.fillRect(x - 1, trackY + 5, pdfScaleX, 16, QColor(229, 57, 53));
          painter.setPen(Qt::white);
        } else {
          if (base == 'A') painter.setPen(Qt::green);
          else if (base == 'C') painter.setPen(Qt::blue);
          else if (base == 'G') painter.setPen(Qt::black);
          else if (base == 'T') painter.setPen(Qt::red);
          else painter.setPen(Qt::gray);
        }
        painter.drawText(x, trackY + 18, QString(base));
        
        if (base != '-' && rawBaseIdx < (int)dispData.qualityScores.size()) {
          int q = dispData.qualityScores[rawBaseIdx];
          QColor qCol = (q >= 30) ? QColor(76, 175, 80) : ((q >= 20) ? QColor(255, 193, 7) : QColor(244, 67, 54));
          painter.fillRect(x, trackY + 21, pdfScaleX - 1, 4, qCol);
          rawBaseIdx++;
        }
      }
      
      currentY += pdfTrackHeight;
    }
    
    currentY += 30;
  }
}

void ContigWidget::fitToWindowHeight(int viewportHeight) {
  if (tracks.empty()) return;
  
  if (viewportHeight <= 0) {
    const QWidget *w = this;
    while (w) {
      const QScrollArea *sa = qobject_cast<const QScrollArea*>(w);
      if (sa && sa->viewport()) {
        viewportHeight = sa->viewport()->height();
        break;
      }
      w = w->parentWidget();
    }
  }
  
  if (viewportHeight <= 80) return;
  
  int numOut = tracks.size() - getNumInContig();
  int extraHeader = trackStartY + (numOut > 0 ? 35 : 0) + 20;
  int availableForTracks = viewportHeight - extraHeader;
  
  if (availableForTracks > 0) {
    trackHeight = std::max(60, availableForTracks / static_cast<int>(tracks.size()));
    updateLayoutGeometry();
    update();
  }
}

void ContigWidget::updateLayoutGeometry() {
  int maxCols = getMaxCols();
  int reqWidth = leftMarginWidth + maxCols * scaleX + 150;
  int reqHeight = getTrackY(tracks.size()) + 60;
  setMinimumSize(reqWidth, reqHeight);
  resize(reqWidth, reqHeight);
}

void ContigWidget::setReference(const QString &seq) {
  pushUndoState();
  referenceSeq = seq;
  isAutoReference = false;
  updateLayoutGeometry();
  update();
}

void ContigWidget::clearAll() {
  pushUndoState();
  referenceSeq.clear();
  consensusSeq.clear();
  tracks.clear();
  selTrack = SEL_NONE;
  searchMatches.clear();
  currentMatchIdx = -1;
  updateLayoutGeometry();
  update();
}

void ContigWidget::runAlignment() {
  if (tracks.empty()) return;
  
  CursorGuard cursorGuard;
  
  pushUndoState();
  
  if (isAutoReference) {
    referenceSeq.clear();
    consensusSeq.clear();
    isAutoReference = false;
    for (auto &tr : tracks) {
      tr.alignment = AlignmentResult();
    }
  }
  
  if (referenceSeq.isEmpty()) {
    size_t maxLen = 0;
    int bestIdx = 0;
    for (size_t i = 0; i < tracks.size(); ++i) {
      QString cleanSeq = tracks[i].originalData.sequence;
      cleanSeq.remove('-');
      if (cleanSeq.length() > maxLen) {
        maxLen = cleanSeq.length();
        bestIdx = i;
      }
    }
    
    if (maxLen == 0) return;
    
    QString seedSeq = tracks[bestIdx].originalData.sequence;
    seedSeq.remove('-');
    
    if (autoRCEnabled) {
      int fwdCount = 0, revCount = 0;
      for (size_t i = 0; i < tracks.size(); ++i) {
        if ((int)i == bestIdx || tracks[i].originalData.sequence.isEmpty()) continue;
        QString cleanRead = tracks[i].originalData.sequence;
        cleanRead.remove('-');
        
        AlignmentResult fwd = AlignmentEngine::alignSemiGlobal(seedSeq, cleanRead, currentAlgorithm, AlignmentEngine::RcMode::ForceForward);
        AlignmentResult rev = AlignmentEngine::alignSemiGlobal(seedSeq, Ab1Parser::reverseComplement(cleanRead), currentAlgorithm, AlignmentEngine::RcMode::ForceForward);
        if (rev.identity > fwd.identity) revCount++;
        else fwdCount++;
      }
      if (revCount > fwdCount) {
        seedSeq = Ab1Parser::reverseComplement(seedSeq);
      }
    } else {
      if (tracks[bestIdx].alignment.isReverseComplement) {
        seedSeq = Ab1Parser::reverseComplement(seedSeq);
      }
    }
    
    referenceSeq = seedSeq;
    isAutoReference = true;
  } else {
    referenceSeq.remove('-');
  }
  
  if (referenceSeq.isEmpty()) return;
  
  int N = referenceSeq.length();
  
  struct TrackAlignData {
    std::vector<QString> refGapInsertions;
    std::vector<QChar> refBaseMatches;
    bool isRC = false;
    double identity = 0.0;
    int score = 0;
  };
  
  std::vector<TrackAlignData> alignData(tracks.size());
  
  for (size_t k = 0; k < tracks.size(); ++k) {
    QString cleanQuery = tracks[k].originalData.sequence;
    cleanQuery.remove('-');
    if (cleanQuery.isEmpty()) continue;
    
    AlignmentEngine::RcMode mode = AlignmentEngine::RcMode::Auto;
    if (!autoRCEnabled) {
      mode = tracks[k].alignment.isReverseComplement ? AlignmentEngine::RcMode::ForceReverse : AlignmentEngine::RcMode::ForceForward;
    } else if (tracks[k].overrideRC) {
      mode = tracks[k].alignment.isReverseComplement ? AlignmentEngine::RcMode::ForceReverse : AlignmentEngine::RcMode::ForceForward;
    }
    
    AlignmentResult res = AlignmentEngine::alignSemiGlobal(referenceSeq, cleanQuery, currentAlgorithm, mode);
    
    alignData[k].isRC = res.isReverseComplement;
    alignData[k].identity = res.identity;
    alignData[k].score = res.score;
    alignData[k].refGapInsertions.resize(N + 1, "");
    alignData[k].refBaseMatches.resize(N, '-');
    
    QString aRef = res.alignedRef;
    QString aRead = res.alignedRead;
    int len = std::min(aRef.length(), aRead.length());
    
    int refIdx = 0;
    for (int p = 0; p < len; ++p) {
      QChar rCh = aRef[p].toUpper();
      QChar qCh = aRead[p].toUpper();
      
      if (rCh != '-') {
        if (refIdx < N) {
          alignData[k].refBaseMatches[refIdx] = qCh;
          refIdx++;
        }
      } else {
        int slot = std::min(refIdx, N);
        alignData[k].refGapInsertions[slot].append(qCh);
      }
    }
  }
  
  std::vector<int> maxGapsPerSlot(N + 1, 0);
  for (int s = 0; s <= N; ++s) {
    int maxG = 0;
    for (size_t k = 0; k < tracks.size(); ++k) {
      maxG = std::max(maxG, (int)alignData[k].refGapInsertions[s].length());
    }
    maxGapsPerSlot[s] = maxG;
  }
  
  QString masterRef = "";
  masterRef.append(QString(maxGapsPerSlot[0], '-'));
  for (int i = 0; i < N; ++i) {
    masterRef.append(referenceSeq[i]);
    masterRef.append(QString(maxGapsPerSlot[i + 1], '-'));
  }
  referenceSeq = masterRef;
  
  for (size_t k = 0; k < tracks.size(); ++k) {
    if (tracks[k].originalData.sequence.isEmpty()) continue;
    
    QString masterRead = "";
    
    QString ins0 = alignData[k].refGapInsertions[0];
    masterRead.append(ins0);
    masterRead.append(QString(maxGapsPerSlot[0] - ins0.length(), '-'));
    
    for (int i = 0; i < N; ++i) {
      masterRead.append(alignData[k].refBaseMatches[i]);
      QString ins = alignData[k].refGapInsertions[i + 1];
      masterRead.append(ins);
      masterRead.append(QString(maxGapsPerSlot[i + 1] - ins.length(), '-'));
    }
    
    tracks[k].alignment.alignedRef = masterRef;
    tracks[k].alignment.alignedRead = masterRead;
    tracks[k].alignment.isReverseComplement = alignData[k].isRC;
    tracks[k].alignment.identity = alignData[k].identity;
    tracks[k].alignment.score = alignData[k].score;
    tracks[k].inContig = (alignData[k].identity >= minIdentityThreshold);
  }
  
  std::stable_sort(tracks.begin(), tracks.end(), [](const AlignedTrack &a, const AlignedTrack &b) {
    return a.inContig > b.inContig;
  });
  
  calculateConsensusAndSNPs();
  searchMatches.clear();
  currentMatchIdx = -1;
  fitToWindowHeight();
  updateLayoutGeometry();
  update();
}

int ContigWidget::getColFromX(int x) const {
  int scrollX = getScrollX();
  if (x < scrollX + leftMarginWidth) return -1;
  return (x - leftMarginWidth) / scaleX;
}

int ContigWidget::getTrackFromY(int y) const {
  if (y >= 5 && y < 32) return SEL_REF;
  if (y >= 32 && y < 45) return SEL_ORF1;
  if (y >= 45 && y < 58) return SEL_ORF2;
  if (y >= 58 && y < 71) return SEL_ORF3;
  if (y >= 71 && y < 84) return SEL_ORF_REV1;
  if (y >= 84 && y < 97) return SEL_ORF_REV2;
  if (y >= 97 && y < 110) return SEL_ORF_REV3;
  if (y >= 110 && y < 145) return SEL_CONSENSUS;
  
  int numIn = getNumInContig();
  int curY = trackStartY;
  
  for (size_t t = 0; t < tracks.size(); ++t) {
    if (t == (size_t)numIn && numIn < (int)tracks.size()) curY += 35;
    if (y >= curY && y < curY + trackHeight) return t;
    curY += trackHeight;
  }
  return SEL_NONE;
}

int ContigWidget::getMaxCols() const {
  int maxC = referenceSeq.length();
  for (const auto &tr : tracks) {
    QString seq = tr.alignment.alignedRead.isEmpty() ? tr.originalData.sequence : tr.alignment.alignedRead;
    maxC = std::max(maxC, (int)seq.length());
  }
  return std::max(maxC, 50);
}

void ContigWidget::zoomInX() { scaleX *= 1.2; updateLayoutGeometry(); update(); }
void ContigWidget::zoomOutX() { scaleX = std::max(2.0, scaleX / 1.2); updateLayoutGeometry(); update(); }
void ContigWidget::zoomInY() { scaleY *= 1.2; updateLayoutGeometry(); update(); }
void ContigWidget::zoomOutY() { scaleY = std::max(0.001, scaleY / 1.2); updateLayoutGeometry(); update(); }
void ContigWidget::increaseTrackHeight() { trackHeight += 15; updateLayoutGeometry(); update(); }
void ContigWidget::decreaseTrackHeight() { trackHeight = std::max(60, trackHeight - 15); updateLayoutGeometry(); update(); }
void ContigWidget::resetZoom() { scaleX = 14.0; scaleY = 0.05; trackHeight = 120; updateLayoutGeometry(); update(); }

void ContigWidget::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  
  painter.fillRect(event->rect(), Qt::white);
  
  int maxCols = getMaxCols();
  int scrollX = getScrollX();
  int numIn = getNumInContig();
  
  // Хелпер нумерации позиций нуклеотидов над прочтением/референсом/консенсусом
  auto drawSequenceCoordinates = [&](int yTop, const QString &seq, QColor col = Qt::darkGray) {
    painter.setFont(QFont("Monospace", 6));
    painter.setPen(col);
    int rawBaseCounter = 0;
    
    for (int colIdx = 0; colIdx < seq.length(); ++colIdx) {
      if (seq[colIdx] != '-') {
        rawBaseCounter++;
        if (rawBaseCounter % 10 == 0 || rawBaseCounter == 1) {
          int x = leftMarginWidth + colIdx * scaleX;
          painter.drawText(x, yTop, QString::number(rawBaseCounter));
        }
      }
    }
  };
  
  struct TrackRange { int first = -1; int last = -1; };
  std::vector<TrackRange> ranges(tracks.size());
  
  for (size_t t = 0; t < tracks.size(); ++t) {
    if (!tracks[t].inContig) continue;
    QString read = tracks[t].alignment.alignedRead.isEmpty() ? 
    tracks[t].originalData.sequence : tracks[t].alignment.alignedRead;
    for (int i = 0; i < read.length(); ++i) {
      if (read[i] != '-') {
        if (ranges[t].first == -1) ranges[t].first = i;
        ranges[t].last = i;
      }
    }
  }
  
  int refFirst = -1, refLast = -1;
  for (int i = 0; i < referenceSeq.length(); ++i) {
    if (referenceSeq[i] != '-') {
      if (refFirst == -1) refFirst = i;
      refLast = i;
    }
  }
  
  for (int col = 0; col < maxCols; ++col) {
    int x = leftMarginWidth + col * scaleX;
    QChar targetBase = '?';
    bool colIsMatch = true;
    int activeCount = 0;
    bool isSnp = std::find(snpColumns.begin(), snpColumns.end(), col) != snpColumns.end();
    
    if (refFirst != -1 && col >= refFirst && col <= refLast) {
      QChar c = referenceSeq[col].toUpper();
      if (c != '-') { targetBase = c; activeCount++; }
    }
    
    for (size_t t = 0; t < tracks.size(); ++t) {
      if (!tracks[t].inContig) continue;
      if (ranges[t].first != -1 && col >= ranges[t].first && col <= ranges[t].last) {
        QString read = tracks[t].alignment.alignedRead.isEmpty() ? 
        tracks[t].originalData.sequence : tracks[t].alignment.alignedRead;
        QChar c = read[col].toUpper();
        if (c == '-') { colIsMatch = false; break; }
        if (targetBase == '?') { targetBase = c; activeCount++; }
        else if (c != targetBase) { colIsMatch = false; break; }
        else { activeCount++; }
      }
    }
    
    if (isSnp) {
      painter.fillRect(x, 0, scaleX, 140, QColor(255, 204, 188, 180));
    } else if (colIsMatch && activeCount >= 2 && targetBase != '?') {
      painter.fillRect(x, 0, scaleX, 140, QColor(255, 245, 157, 180));
    }
  }
  
  if (selTrack != SEL_NONE && selStartCol >= 0 && selEndCol >= 0) {
    int minC = std::min(selStartCol, selEndCol);
    int maxC = std::max(selStartCol, selEndCol);
    int x = leftMarginWidth + minC * scaleX;
    int w = (maxC - minC + 1) * scaleX;
    
    int y = 0, h = 0;
    if (selTrack == SEL_REF) { y = 14; h = 18; }
    else if (selTrack == SEL_ORF1) { y = 33; h = 12; }
    else if (selTrack == SEL_ORF2) { y = 46; h = 12; }
    else if (selTrack == SEL_ORF3) { y = 59; h = 12; }
    else if (selTrack == SEL_ORF_REV1) { y = 72; h = 12; }
    else if (selTrack == SEL_ORF_REV2) { y = 85; h = 12; }
    else if (selTrack == SEL_ORF_REV3) { y = 98; h = 12; }
    else if (selTrack == SEL_CONSENSUS) { y = 118; h = 20; }
    else if (selTrack >= 0 && selTrack < (int)tracks.size()) {
      y = getTrackY(selTrack);
      h = trackHeight - 5;
    }
    
    if (h > 0) {
      painter.fillRect(x, y, w, h, QColor(33, 150, 243, 100));
      painter.setPen(QPen(Qt::blue, 1, Qt::DashLine));
      painter.drawRect(x, y, w, h);
    }
  }
  
  drawSequenceCoordinates(12, referenceSeq, Qt::darkGreen);
  
  int startY = 25;
  QFont seqFont("Monospace", 10, QFont::Bold);
  painter.setFont(seqFont);
  
  for (int i = 0; i < referenceSeq.length(); ++i) {
    int x = leftMarginWidth + i * scaleX;
    QChar base = referenceSeq[i].toUpper();
    if (base == 'A') painter.setPen(Qt::green);
    else if (base == 'C') painter.setPen(Qt::blue);
    else if (base == 'G') painter.setPen(Qt::black);
    else if (base == 'T') painter.setPen(Qt::red);
    else painter.setPen(Qt::gray);
    painter.drawText(x, startY, QString(base));
  }
  
  // 6 Рамок Считывания (ORF +1, +2, +3, -1, -2, -3)
  int orfY[6] = { 42, 55, 68, 81, 94, 107 };
  QFont orfFont("Monospace", 8, QFont::Bold);
  painter.setFont(orfFont);
  
  for (int frame = 0; frame < 6; ++frame) {
    int yPos = orfY[frame];
    QString orfSeq = getOrfLine(frame);
    for (int col = 0; col < orfSeq.length(); ++col) {
      QChar aa = orfSeq[col];
      if (aa != ' ') {
        int x = leftMarginWidth + col * scaleX;
        if (aa == 'M') painter.setPen(Qt::darkGreen);
        else if (aa == '*') painter.setPen(Qt::red);
        else if (frame >= 3) painter.setPen(QColor(230, 81, 0)); // Обратные рамки подсвечиваем оранжевым
        else painter.setPen(Qt::darkBlue);
        painter.drawText(x, yPos, QString(aa));
      }
    }
  }
  
  drawSequenceCoordinates(120, consensusSeq, QColor(156, 39, 176));
  
  startY = 135;
  painter.setFont(seqFont);
  
  for (int i = 0; i < consensusSeq.length(); ++i) {
    int x = leftMarginWidth + i * scaleX;
    QChar base = consensusSeq[i].toUpper();
    if (base == 'A') painter.setPen(Qt::green);
    else if (base == 'C') painter.setPen(Qt::blue);
    else if (base == 'G') painter.setPen(Qt::black);
    else if (base == 'T') painter.setPen(Qt::red);
    else if (base == '-') painter.setPen(Qt::gray);
    else painter.setPen(QColor(156, 39, 176));
    painter.drawText(x, startY, QString(base));
  }
  
  int currentY = trackStartY;
  
  for (size_t t = 0; t < tracks.size(); ++t) {
    if (t == (size_t)numIn && numIn < (int)tracks.size()) {
      int sepY = currentY + 15;
      painter.setPen(QPen(Qt::red, 2, Qt::SolidLine));
      painter.drawLine(0, sepY, width(), sepY);
      painter.fillRect(scrollX + leftMarginWidth + 10, sepY - 12, 480, 24, Qt::white);
      painter.setPen(Qt::red);
      QFont sepFont("SansSerif", 9, QFont::Bold); painter.setFont(sepFont);
      painter.drawText(scrollX + leftMarginWidth + 15, sepY + 4, 
                       QString("⛔ Не включены в контиг (Identity < %1%)").arg(minIdentityThreshold, 0, 'f', 1));
      currentY += 35;
    }
    
    auto &track = tracks[t];
    Ab1Data dispData = Ab1Parser::getOrientedData(track.originalData, track.alignment.isReverseComplement);
    QString readAligned = track.alignment.alignedRead.isEmpty() ? dispData.sequence : track.alignment.alignedRead;
    
    drawSequenceCoordinates(currentY + 5, readAligned, Qt::gray);
    
    struct BasePoint { double x; int col; };
    std::vector<BasePoint> basePoints;
    for (int col = 0; col < readAligned.length(); ++col) {
      if (readAligned[col] != '-') {
        basePoints.push_back({leftMarginWidth + col * scaleX, col});
      }
    }
    
    int rawBaseIdx = 0;
    
    for (int col = 0; col < readAligned.length(); ++col) {
      int x = leftMarginWidth + col * scaleX;
      QChar base = readAligned[col];
      QChar refC = (col < referenceSeq.length()) ? referenceSeq[col].toUpper() : '?';
      
      bool isMismatchLetter = (base != '-' && refC != '?' && refC != '-' && base.toUpper() != refC);
      
      if (isMismatchLetter) {
        painter.fillRect(x - 1, currentY + 7, scaleX, 15, QColor(229, 57, 53));
        painter.setPen(Qt::white);
      } else {
        if (base == 'A') painter.setPen(Qt::green);
        else if (base == 'C') painter.setPen(Qt::blue);
        else if (base == 'G') painter.setPen(Qt::black);
        else if (base == 'T') painter.setPen(Qt::red);
        else painter.setPen(Qt::gray);
      }
      
      painter.drawText(x, currentY + 20, QString(base));
      
      if (base != '-' && rawBaseIdx < (int)dispData.qualityScores.size()) {
        int q = dispData.qualityScores[rawBaseIdx];
        QColor qCol = (q >= 30) ? QColor(76, 175, 80) : ((q >= 20) ? QColor(255, 193, 7) : QColor(244, 67, 54));
        painter.fillRect(x, currentY + 23, scaleX - 1, 5, qCol);
        rawBaseIdx++;
      }
    }
    
    const auto &ploc = dispData.basePositions;
    size_t numBases = std::min(basePoints.size(), ploc.size());
    
    if (numBases >= 2) {
      int baseline = currentY + trackHeight - 5;
      auto drawContinuousTrace = [&](const std::vector<int>& trace, QColor col) {
        if (trace.empty()) return;
        QPainterPath path;
        
        for (size_t bp = 0; bp + 1 < numBases; ++bp) {
          int startS = ploc[bp];
          int endS = ploc[bp + 1];
          if (startS >= (int)trace.size()) continue;
          endS = std::min(endS, (int)trace.size() - 1);
          if (startS >= endS) continue;
          
          bool hasGap = (basePoints[bp + 1].col > basePoints[bp].col + 1);
          int midS = (startS + endS) / 2;
          
          for (int s = startS; s <= endS; ++s) {
            double xPos;
            if (!hasGap) {
              double t_param = double(s - startS) / std::max(1, endS - startS);
              xPos = basePoints[bp].x + t_param * (basePoints[bp + 1].x - basePoints[bp].x);
            } else {
              if (s <= midS) {
                double t_param = double(s - startS) / std::max(1, midS - startS);
                xPos = basePoints[bp].x + t_param * (scaleX * 0.5);
              } else {
                double t_param = double(s - midS) / std::max(1, endS - midS);
                xPos = (basePoints[bp + 1].x - scaleX * 0.5) + t_param * (scaleX * 0.5);
              }
            }
            
            double yPos = baseline - trace[s] * scaleY;
            
            if (bp == 0 && s == startS) {
              path.moveTo(xPos, yPos);
            } else if (hasGap && s == midS + 1) {
              path.moveTo(xPos, baseline);
              path.lineTo(xPos, yPos);
            } else {
              path.lineTo(xPos, yPos);
            }
          }
        }
        painter.setPen(QPen(col, 1.4));
        painter.drawPath(path);
      };
      
      drawContinuousTrace(dispData.traceA, Qt::green);
      drawContinuousTrace(dispData.traceC, Qt::blue);
      drawContinuousTrace(dispData.traceG, Qt::black);
      drawContinuousTrace(dispData.traceT, Qt::red);
    }
    painter.setPen(Qt::lightGray);
    painter.drawLine(0, currentY + trackHeight - 5, width(), currentY + trackHeight - 5);
    currentY += trackHeight;
  }
  
  painter.fillRect(scrollX, 0, leftMarginWidth - 5, height(), Qt::white);
  painter.setPen(QPen(Qt::gray, 2));
  painter.drawLine(scrollX + leftMarginWidth - 5, 0, scrollX + leftMarginWidth - 5, height());
  
  painter.setFont(seqFont);
  painter.setPen(Qt::darkGreen);
  painter.drawText(scrollX + 10, 25, isAutoReference ? "CONTIG (Auto):" : "REF (FASTA):");
  
  painter.setFont(QFont("SansSerif", 7));
  painter.setPen(Qt::darkGray);
  painter.drawText(scrollX + 10, 42, "ORF +1:");
  painter.drawText(scrollX + 10, 55, "ORF +2:");
  painter.drawText(scrollX + 10, 68, "ORF +3:");
  painter.setPen(QColor(230, 81, 0));
  painter.drawText(scrollX + 10, 81, "ORF -1:");
  painter.drawText(scrollX + 10, 94, "ORF -2:");
  painter.drawText(scrollX + 10, 107, "ORF -3:");
  
  painter.setFont(seqFont);
  painter.setPen(QColor(156, 39, 176));
  painter.drawText(scrollX + 10, 135, "CONSENSUS:");
  
  QFont labelFont("SansSerif", 9);
  for (size_t t = 0; t < tracks.size(); ++t) {
    int tY = getTrackY(t);
    QRect nameRect(scrollX + 10, tY, leftMarginWidth - 25, trackHeight - 5);
    painter.setFont(labelFont);
    painter.setPen(tracks[t].inContig ? Qt::darkBlue : Qt::red);
    
    QString rcMark = tracks[t].alignment.isReverseComplement ? " [RC]" : "";
    QString labelText = QString("%1%2 (%3%)")
      .arg(tracks[t].fileName)
      .arg(rcMark)
      .arg(tracks[t].alignment.identity, 0, 'f', 1);
    
    painter.drawText(nameRect, Qt::TextWordWrap | Qt::TextWrapAnywhere | Qt::AlignLeft | Qt::AlignTop, labelText);
  }
}

void ContigWidget::leaveEvent(QEvent *event) {
  QWidget::leaveEvent(event);
  if (hoverCol != -1) {
    hoverCol = -1;
    update();
  }
}

void ContigWidget::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void ContigWidget::dropEvent(QDropEvent *event) {
  for (const QUrl &url : event->mimeData()->urls()) {
    QString filePath = url.toLocalFile();
    if (filePath.isEmpty()) continue;
    
    if (filePath.endsWith(".ChromaZ", Qt::CaseInsensitive)) {
      loadProject(filePath);
      return;
    }
    
    if (filePath.endsWith(".fastq", Qt::CaseInsensitive) || filePath.endsWith(".fq", Qt::CaseInsensitive)) {
      auto records = SequenceFileParser::parseFastq(filePath);
      for (const auto &rec : records) {
        QString trackId = (records.size() > 1) ? QString("%1 [%2]").arg(QFileInfo(filePath).fileName()).arg(rec.name) : filePath;
        addAb1Track(trackId, rec.data);
      }
      continue;
    }
    
    if (filePath.endsWith(".txt", Qt::CaseInsensitive)) {
      auto records = SequenceFileParser::parseTxt(filePath);
      for (const auto &rec : records) {
        QString trackId = (records.size() > 1) ? QString("%1 [%2]").arg(QFileInfo(filePath).fileName()).arg(rec.name) : filePath;
        addAb1Track(trackId, rec.data);
      }
      continue;
    }
    
    if (isFileLoaded(filePath)) continue;
    
    if (filePath.endsWith(".ab1", Qt::CaseInsensitive) || filePath.endsWith(".scf", Qt::CaseInsensitive)) {
      Ab1Data data = Ab1Parser::parse(filePath);
      if (data.isValid) addAb1Track(filePath, data);
    } else if (filePath.endsWith(".fasta", Qt::CaseInsensitive) || 
      filePath.endsWith(".fa", Qt::CaseInsensitive) ||
      filePath.endsWith(".gb", Qt::CaseInsensitive) ||
      filePath.endsWith(".gbk", Qt::CaseInsensitive)) {
      auto records = SequenceFileParser::parseFastaOrGbRecords(filePath);
      for (const auto &rec : records) {
        QString trackId = (records.size() > 1) ? QString("%1 [%2]").arg(QFileInfo(filePath).fileName()).arg(rec.name) : filePath;
        addAb1Track(trackId, rec.data);
      }
    }
  }
  runAlignment();
}

void ContigWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    int x = event->position().x();
    int scrollX = getScrollX();
    
    if (std::abs(x - (scrollX + leftMarginWidth - 5)) <= 8) {
      isResizingMargin = true; return;
    }
    
    int c = getColFromX(x);
    int t = getTrackFromY(event->position().y());
    
    if (x < scrollX + leftMarginWidth && t >= 0) {
      dragSourceTrackIdx = t; return;
    }
    
    if (c >= 0 && t != SEL_NONE) {
      selTrack = t; selStartCol = c; selEndCol = c;
      isSelecting = true; update();
    }
  }
}

void ContigWidget::mouseMoveEvent(QMouseEvent *event) {
  int x = event->position().x(), scrollX = getScrollX(), y = event->position().y();
  
  int oldHover = hoverCol;
  if (!isSelecting && !isResizingMargin && dragSourceTrackIdx < 0) {
    hoverCol = getColFromX(x);
  } else {
    hoverCol = -1;
  }
  
  if (dragSourceTrackIdx >= 0 && (event->buttons() & Qt::LeftButton)) {
    int targetTrack = getTrackFromY(y);
    if (targetTrack >= 0 && targetTrack < (int)tracks.size() && targetTrack != dragSourceTrackIdx) {
      if (tracks[dragSourceTrackIdx].inContig == tracks[targetTrack].inContig) {
        pushUndoState();
        std::swap(tracks[dragSourceTrackIdx], tracks[targetTrack]);
        dragSourceTrackIdx = targetTrack;
        update();
      }
    }
    return;
  }
  
  if (isResizingMargin) {
    leftMarginWidth = std::clamp(x - scrollX, 80, 800);
    updateLayoutGeometry(); update(); return;
  }
  
  if (std::abs(x - (scrollX + leftMarginWidth - 5)) <= 8) setCursor(Qt::SplitHCursor);
  else if (!isSelecting) unsetCursor();
  
  if (isSelecting) {
    int c = getColFromX(x);
    if (c >= 0) { selEndCol = c; update(); }
  }
  
  if (hoverCol != oldHover) update();
}

void ContigWidget::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    dragSourceTrackIdx = -2;
    if (isResizingMargin) { isResizingMargin = false; unsetCursor(); }
    isSelecting = false;
  }
}

void ContigWidget::keyPressEvent(QKeyEvent *event) {
  if (event->matches(QKeySequence::Undo)) { undo(); return; }
  if (event->matches(QKeySequence::Redo)) { redo(); return; }
  if (event->matches(QKeySequence::Copy)) { copySelectedToClipboard(); return; }
  
  if (selTrack == SEL_NONE || selStartCol < 0) return;
  
  int col = selStartCol;
  QString text = event->text().toUpper();
  
  if (selTrack == SEL_REF) {
    if (!text.isEmpty() && (text == "A" || text == "C" || text == "G" || text == "T" || text == "N" || text == "-")) {
      pushUndoState();
      if (col < referenceSeq.length()) referenceSeq[col] = text[0];
      else referenceSeq.append(text[0]);
      calculateConsensusAndSNPs(); updateLayoutGeometry(); update();
    } else if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
      if (col < referenceSeq.length()) {
        pushUndoState();
        if (referenceSeq[col] != '-') referenceSeq[col] = '-';
        else referenceSeq.remove(col, 1);
        calculateConsensusAndSNPs(); updateLayoutGeometry(); update();
      }
    } else if (event->key() == Qt::Key_Left) {
      selStartCol = std::max(0, selStartCol - 1); selEndCol = selStartCol; update();
    } else if (event->key() == Qt::Key_Right) {
      selStartCol++; selEndCol = selStartCol; update();
    }
    return;
  }
  
  if (selTrack == SEL_CONSENSUS) {
    if (!text.isEmpty() && (text == "A" || text == "C" || text == "G" || text == "T" || text == "N" || text == "-")) {
      pushUndoState();
      if (col < consensusSeq.length()) consensusSeq[col] = text[0];
      else consensusSeq.append(text[0]);
      update();
    } else if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
      if (col < consensusSeq.length()) {
        pushUndoState();
        if (consensusSeq[col] != '-') consensusSeq[col] = '-';
        else consensusSeq.remove(col, 1);
        update();
      }
    } else if (event->key() == Qt::Key_Left) {
      selStartCol = std::max(0, selStartCol - 1); selEndCol = selStartCol; update();
    } else if (event->key() == Qt::Key_Right) {
      selStartCol++; selEndCol = selStartCol; update();
    }
    return;
  }
  
  if (selTrack >= 0 && selTrack < (int)tracks.size()) {
    auto &track = tracks[selTrack];
    auto &read = track.alignment.alignedRead;
    auto &origSeq = track.originalData.sequence;
    auto &qScores = track.originalData.qualityScores;
    auto &bPos = track.originalData.basePositions;
    bool isRC = track.alignment.isReverseComplement;
    
    if (event->key() == Qt::Key_Left) {
      selStartCol = std::max(0, selStartCol - 1); selEndCol = selStartCol; update();
      return;
    } else if (event->key() == Qt::Key_Right) {
      selStartCol++; selEndCol = selStartCol; update();
      return;
    }
    
    if (col >= read.length()) return;
    
    int rawIdx = 0;
    for (int i = 0; i < col && i < read.length(); ++i) {
      if (read[i] != '-') rawIdx++;
    }
    
    if (!text.isEmpty() && (text == "A" || text == "C" || text == "G" || text == "T" || text == "N" || text == "-")) {
      pushUndoState();
      QChar newChar = text[0];
      QChar oldChar = read[col];
      
      read[col] = newChar;
      
      QChar origNewChar = isRC ? Ab1Parser::reverseComplement(QString(newChar))[0] : newChar;
      
      if (oldChar != '-' && newChar != '-') {
        int origIdx = isRC ? ((int)origSeq.length() - 1 - rawIdx) : rawIdx;
        if (origIdx >= 0 && origIdx < origSeq.length()) {
          origSeq[origIdx] = origNewChar;
        }
      } else if (oldChar == '-' && newChar != '-') {
        int origIdx = isRC ? ((int)origSeq.length() - rawIdx) : rawIdx;
        if (origIdx >= 0 && origIdx <= origSeq.length()) {
          origSeq.insert(origIdx, origNewChar);
        }
        int qIdx = isRC ? ((int)qScores.size() - rawIdx) : rawIdx;
        if (qIdx >= 0 && qIdx <= (int)qScores.size()) {
          qScores.insert(qScores.begin() + qIdx, 30);
        }
        int pIdx = isRC ? ((int)bPos.size() - rawIdx) : rawIdx;
        if (pIdx >= 0 && pIdx <= (int)bPos.size()) {
          int posVal = (pIdx > 0 && pIdx - 1 < (int)bPos.size()) ? bPos[pIdx - 1] + 10 : 10;
          bPos.insert(bPos.begin() + pIdx, posVal);
        }
      } else if (oldChar != '-' && newChar == '-') {
        int origIdx = isRC ? ((int)origSeq.length() - 1 - rawIdx) : rawIdx;
        if (origIdx >= 0 && origIdx < origSeq.length()) {
          origSeq.remove(origIdx, 1);
        }
        int qIdx = isRC ? ((int)qScores.size() - 1 - rawIdx) : rawIdx;
        if (qIdx >= 0 && qIdx < (int)qScores.size()) {
          qScores.erase(qScores.begin() + qIdx);
        }
        int pIdx = isRC ? ((int)bPos.size() - 1 - rawIdx) : rawIdx;
        if (pIdx >= 0 && pIdx < (int)bPos.size()) {
          bPos.erase(bPos.begin() + pIdx);
        }
      }
      calculateConsensusAndSNPs(); updateLayoutGeometry(); update();
    } else if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
      pushUndoState();
      QChar oldChar = read[col];
      
      if (oldChar != '-') {
        read[col] = '-';
        
        int origIdx = isRC ? ((int)origSeq.length() - 1 - rawIdx) : rawIdx;
        if (origIdx >= 0 && origIdx < origSeq.length()) {
          origSeq.remove(origIdx, 1);
        }
        int qIdx = isRC ? ((int)qScores.size() - 1 - rawIdx) : rawIdx;
        if (qIdx >= 0 && qIdx < (int)qScores.size()) {
          qScores.erase(qScores.begin() + qIdx);
        }
        int pIdx = isRC ? ((int)bPos.size() - 1 - rawIdx) : rawIdx;
        if (pIdx >= 0 && pIdx < (int)bPos.size()) {
          bPos.erase(bPos.begin() + pIdx);
        }
      } else {
        read.remove(col, 1);
      }
      calculateConsensusAndSNPs(); updateLayoutGeometry(); update();
    }
  }
}

void ContigWidget::copySelectedToClipboard() {
  if (selTrack == SEL_NONE || selStartCol < 0 || selEndCol < 0) return;
  int minC = std::min(selStartCol, selEndCol), maxC = std::max(selStartCol, selEndCol);
  int len = maxC - minC + 1;
  QString seq = "";
  
  if (selTrack == SEL_REF) seq = referenceSeq.mid(minC, len);
  else if (selTrack == SEL_ORF1) seq = getOrfLine(0).mid(minC, len);
  else if (selTrack == SEL_ORF2) seq = getOrfLine(1).mid(minC, len);
  else if (selTrack == SEL_ORF3) seq = getOrfLine(2).mid(minC, len);
  else if (selTrack == SEL_ORF_REV1) seq = getOrfLine(3).mid(minC, len);
  else if (selTrack == SEL_ORF_REV2) seq = getOrfLine(4).mid(minC, len);
  else if (selTrack == SEL_ORF_REV3) seq = getOrfLine(5).mid(minC, len);
  else if (selTrack == SEL_CONSENSUS) seq = consensusSeq.mid(minC, len);
  else if (selTrack >= 0 && selTrack < (int)tracks.size()) {
    QString full = tracks[selTrack].alignment.alignedRead.isEmpty() ? 
    tracks[selTrack].originalData.sequence : 
    tracks[selTrack].alignment.alignedRead;
    seq = full.mid(minC, len);
  }
  if (!seq.isEmpty()) QGuiApplication::clipboard()->setText(seq);
}

// КОПИРОВАНИЕ ВЫДЕЛЕННОГО УЧАСТКА ХРОМАТОГРАММЫ В КЛИПБОРД КАК ИЗОБРАЖЕНИЕ (PNG)
void ContigWidget::copySelectionAsImageToClipboard() {
  if (selStartCol < 0 || selEndCol < 0) return;
  
  int minC = std::min(selStartCol, selEndCol);
  int maxC = std::max(selStartCol, selEndCol);
  int numCols = maxC - minC + 1;
  
  int imgWidth = numCols * scaleX + leftMarginWidth;
  int imgHeight = getTrackY(tracks.size()) + 20;
  
  QImage image(imgWidth, imgHeight, QImage::Format_ARGB32);
  image.fill(Qt::white);
  
  QPainter imgPainter(&image);
  imgPainter.setRenderHint(QPainter::Antialiasing);
  imgPainter.translate(-minC * scaleX, 0);
  
  render(&imgPainter);
  QGuiApplication::clipboard()->setImage(image);
}

void ContigWidget::contextMenuEvent(QContextMenuEvent *event) {
  QMenu menu(this);
  QAction *copyAct = menu.addAction("📋 Copy Sequence (Ctrl+C)");
  connect(copyAct, &QAction::triggered, this, &ContigWidget::copySelectedToClipboard);
  
  QAction *copyImgAct = menu.addAction("🖼 Copy Selection as Image");
  connect(copyImgAct, &QAction::triggered, this, &ContigWidget::copySelectionAsImageToClipboard);
  
  int t = getTrackFromY(event->pos().y());
  if (t >= 0 && t < (int)tracks.size()) {
    menu.addSeparator();
    
    QString rcTitle = tracks[t].alignment.isReverseComplement ? "🔄 Revert to Forward Strand" : "🔄 Convert to Reverse Complement";
    QAction *toggleRCAct = menu.addAction(rcTitle);
    connect(toggleRCAct, &QAction::triggered, this, [this, t]() {
      pushUndoState();
      tracks[t].alignment.isReverseComplement = !tracks[t].alignment.isReverseComplement;
      tracks[t].overrideRC = true;
      runAlignment();
    });
    
    QAction *deleteTrackAct = menu.addAction("❌ Delete Track");
    connect(deleteTrackAct, &QAction::triggered, this, [this, t]() {
      if (t >= 0 && t < (int)tracks.size()) {
        pushUndoState();
        tracks.erase(tracks.begin() + t);
        calculateConsensusAndSNPs();
        fitToWindowHeight();
        updateLayoutGeometry();
        update();
      }
    });
  }
  
  menu.exec(event->globalPos());
}

void ContigWidget::wheelEvent(QWheelEvent *event) {
  if (event->modifiers() & Qt::ControlModifier) {
    if (event->angleDelta().y() > 0) zoomInY(); else zoomOutY();
  } else {
    if (event->angleDelta().y() > 0) zoomInX(); else zoomOutX();
  }
}