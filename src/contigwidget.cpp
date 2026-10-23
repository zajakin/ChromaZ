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
#include <QByteArray>
#include <QSaveFile>
#include "projectio.h"
#include <QLocale>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QKeySequence>
#include <QJsonParseError>
#include <utility>
#include <iterator>
#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>
#include <QMessageBox>
#include <QPrintDialog>
#include <QPrinter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTabWidget>
#include <QDialog>
#include <QDateTime>
#include <QFileDialog>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QImage>
#include <QListView>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QSettings>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include "alignjob.h"
#include "alignworker.h"
#include "tmcalc.h"
#include "virtualgel.h"
#include <map>
#include <cmath>
#include <algorithm>
#include <cctype>

namespace {
struct CursorGuard {
  CursorGuard() { QGuiApplication::setOverrideCursor(Qt::WaitCursor); }
  ~CursorGuard() { QGuiApplication::restoreOverrideCursor(); }
};
  
  double getAAMolecularWeight(QChar aa) {
    switch (aa.toUpper().toLatin1()) {
    case 'A': return 71.04; case 'R': return 156.19; case 'N': return 114.10;
    case 'D': return 115.09; case 'C': return 103.14; case 'E': return 129.12;
    case 'Q': return 128.13; case 'G': return 57.05;  case 'H': return 137.14;
    case 'I': return 113.16; case 'L': return 113.16; case 'K': return 128.17;
    case 'M': return 131.19; case 'F': return 147.18; case 'P': return 97.12;
    case 'S': return 87.08;  case 'T': return 101.11; case 'W': return 186.21;
    case 'Y': return 163.18; case 'V': return 99.13;  default: return 0.0;
    }
  }
  
  double getKyteDoolittle(QChar aa) {
    switch (aa.toUpper().toLatin1()) {
    case 'A': return 1.8;  case 'R': return -4.5; case 'N': return -3.5;
    case 'D': return -3.5; case 'C': return 2.5;  case 'Q': return -3.5;
    case 'E': return -3.5; case 'G': return -0.4; case 'H': return -3.2;
    case 'I': return 4.5;  case 'L': return 3.8;  case 'K': return -3.9;
    case 'M': return 1.9;  case 'F': return 2.8;  case 'P': return -1.6;
    case 'S': return -0.8; case 'T': return -0.7; case 'W': return -0.9;
    case 'Y': return -1.3; case 'V': return 4.2;  default: return 0.0;
    }
  }
  
  QString formatSequenceWithCoordinates(const QString &seq, int lineLength = 60, int blockSize = 10) {
    QString out;
    int n = seq.length();
    int maxCoordDigits = QString::number(n).length();
    int labelWidth = std::max(6, maxCoordDigits + 2);
    
    for (int i = 0; i < n; i += lineLength) {
      int coord = i + 1;
      out += QString("%1 ").arg(coord, labelWidth, 10, QChar(' '));
      for (int j = 0; j < lineLength && i + j < n; j += blockSize) {
        out += seq.mid(i + j, blockSize) + " ";
      }
      out.chop(1);
      out += "\n";
    }
    return out;
  }
  
  class SequenceGraphWidget : public QWidget {
  public:
    QVector<double> values;
    QString sequence;
    QColor lineColor;
    double minV = 0.0, maxV = 1.0;
    double baselineVal = 0.0;
    bool showBaseline = true;
    QString topLabel;
    QString bottomLabel;
    double baseStepX = 18.0;
    
    SequenceGraphWidget(QColor color, QWidget *parent = nullptr)
      : QWidget(parent), lineColor(color) {
      setMinimumHeight(180);
    }
    
    void updateDimensions(int availableWidth) {
      if (values.isEmpty()) {
        resize(availableWidth, 180);
        return;
      }
      double neededWidth = 60 + values.size() * baseStepX + 40;
      int finalWidth = std::max((int)neededWidth, availableWidth);
      setMinimumWidth(finalWidth);
      resize(finalWidth, 180);
    }
    
    void paintEvent(QPaintEvent *) override {
      QPainter p(this);
      p.setRenderHint(QPainter::Antialiasing);
      p.fillRect(rect(), Qt::white);
      
      if (values.isEmpty()) return;
      
      int leftM = 50;
      int rightM = 30;
      int topM = 20;
      int bottomM = 40;
      int plotH = height() - topM - bottomM;
      int plotW = width() - leftM - rightM;
      
      if (plotH <= 10 || plotW <= 10) return;
      
      double range = maxV - minV;
      if (range <= 0.0) range = 1.0;
      
      auto toY = [&](double v) -> double {
        double norm = (v - minV) / range;
        return topM + plotH - norm * plotH;
      };
      
      double step = double(plotW) / std::max(1, (int)values.size());
      
      p.setPen(QColor(230, 230, 230));
      p.drawRect(leftM, topM, plotW, plotH);
      
      if (showBaseline && baselineVal >= minV && baselineVal <= maxV) {
        double baseLineY = toY(baselineVal);
        p.setPen(QPen(QColor(180, 180, 180), 1, Qt::DashLine));
        p.drawLine(leftM, baseLineY, leftM + plotW, baseLineY);
      }
      
      QFont fontSmall("SansSerif", 7);
      p.setFont(fontSmall);
      p.setPen(Qt::darkGray);
      p.drawText(5, toY(maxV) + 4, QString::number(maxV, 'f', 1));
      p.drawText(5, toY(minV) + 4, QString::number(minV, 'f', 1));
      if (showBaseline) {
        p.drawText(5, toY(baselineVal) + 4, QString::number(baselineVal, 'f', 1));
      }
      if (!topLabel.isEmpty()) {
        p.setPen(QColor(100, 100, 100));
        p.drawText(leftM + 5, topM - 5, topLabel);
      }
      if (!bottomLabel.isEmpty()) {
        p.setPen(QColor(100, 100, 100));
        p.drawText(leftM + 5, topM + plotH - 5, bottomLabel);
      }
      
      QFont seqFont("Monospace", 8, QFont::Bold);
      QFont tickFont("SansSerif", 6);
      double seqY = topM + plotH + 18;
      double tickY = topM + plotH + 32;
      
      for (int i = 0; i < values.size(); ++i) {
        double x = leftM + (i + 0.5) * step;
        
        p.setPen(QColor(210, 210, 210));
        p.drawLine(x, topM + plotH, x, topM + plotH + 4);
        
        if (!sequence.isEmpty() && i < sequence.length()) {
          p.setFont(seqFont);
          QChar ch = sequence[i].toUpper();
          if (ch == 'R' || ch == 'K' || ch == 'D' || ch == 'E' || ch == 'H') {
            p.setPen(QColor(33, 150, 243));
          } else if (ch == 'I' || ch == 'V' || ch == 'L' || ch == 'F' || ch == 'W' || ch == 'M' || ch == 'A') {
            p.setPen(QColor(230, 81, 0));
          } else {
            p.setPen(Qt::darkGray);
          }
          QRect charRect(x - step / 2, seqY - 12, step, 16);
          p.drawText(charRect, Qt::AlignCenter, QString(ch));
        }
        
        int pos = i + 1;
        bool drawNum = (pos == 1) || (step >= 12 && pos % 5 == 0) || (pos % 10 == 0);
        if (drawNum) {
          p.setFont(tickFont);
          p.setPen(Qt::gray);
          QRect numRect(x - 20, tickY - 10, 40, 12);
          p.drawText(numRect, Qt::AlignCenter, QString::number(pos));
        }
      }
      
      QPainterPath path;
      for (int i = 0; i < values.size(); ++i) {
        double x = leftM + (i + 0.5) * step;
        double y = toY(values[i]);
        if (i == 0) path.moveTo(x, y);
        else path.lineTo(x, y);
      }
      
      p.setPen(QPen(lineColor, 2));
      p.drawPath(path);
      
      if (step >= 8.0) {
        p.setBrush(lineColor);
        for (int i = 0; i < values.size(); ++i) {
          double x = leftM + (i + 0.5) * step;
          double y = toY(values[i]);
          p.drawEllipse(QPointF(x, y), 2.5, 2.5);
        }
      }
    }
  };
} // namespace

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
  
  tts = new QTextToSpeech(this);
  tts->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
  tts->setRate(speechRate);
  
  loadRestrictionEnzymes();
  updateLayoutGeometry();
}

ContigWidget::~ContigWidget() { blockSignals(true); abortAlignment(); }

QList<QVoice> ContigWidget::getAvailableVoices() const {
  if (tts) return tts->availableVoices();
  return {};
}

QVoice ContigWidget::getCurrentVoice() const {
  if (tts) return tts->voice();
  return QVoice();
}

void ContigWidget::setVoice(const QVoice &voice) {
  if (tts) {
    tts->setVoice(voice);
    emit voiceChanged(voice);
  }
}

void ContigWidget::loadRestrictionEnzymes() {
  restrictionEnzymes.clear();
  QString localPath = QCoreApplication::applicationDirPath() + "/bionetc.txt";
  QFile file(localPath);
  if (!file.exists()) file.setFileName(":/bionetc.txt");
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  
  QTextStream in(&file);
  static const QRegularExpression iisRegex(R"(^([A-Za-z]+)\((-?\d+)/(-?\d+)\)$)");
  static const QRegularExpression validIupacRegex(R"(^[ACGTURYMKWSBDHVN]+$)", QRegularExpression::CaseInsensitiveOption);
  
  QSettings settings("ChromaZ", "ChromaZ");
  QStringList disabledList = settings.value("disabledEnzymes").toStringList();
  
  while (!in.atEnd()) {
    QString line = in.readLine().trimmed();
    if (line.isEmpty()) continue;
    
    if (line.startsWith('#') || line.startsWith("..") || line.startsWith("=-=") ||
        line.startsWith("REBASE", Qt::CaseInsensitive) ||
        line.contains("Copyright", Qt::CaseInsensitive) ||
        line.contains("Rich Roberts", Qt::CaseInsensitive) ||
        line.contains("http://", Qt::CaseInsensitive)) {
      continue;
    }
    
    QStringList parts = line.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
    if (parts.size() < 2) continue;
    
    QString rawSite = parts.last();
    parts.removeLast();
    QString name = parts.join(" ");
    
    QString cleanMotif = rawSite;
    cleanMotif.remove(QRegularExpression(R"([^A-Za-z])"));
    
    if (cleanMotif.length() < 3) continue;
    if (!validIupacRegex.match(cleanMotif).hasMatch()) continue;
    
    RestEnzyme enz;
    enz.name = name;
    enz.enabled = !disabledList.contains(name);
    
    auto match = iisRegex.match(rawSite);
    if (match.hasMatch()) {
      enz.motif = match.captured(1).toUpper();
      int offset = match.captured(2).toInt();
      enz.cutIdx = enz.motif.length() + offset;
    } else {
      int cut = rawSite.indexOf('^');
      if (cut == -1) cut = rawSite.indexOf('\'');
      if (cut == -1) cut = rawSite.indexOf('_');
      
      if (cut != -1) {
        enz.cutIdx = cut;
      } else {
        enz.cutIdx = cleanMotif.length() / 2;
      }
      enz.motif = cleanMotif.toUpper();
    }
    
    if (!enz.motif.isEmpty()) {
      restrictionEnzymes.push_back(enz);
    }
  }
}

void ContigWidget::showEnzymeSelectionDialog() {
  if (restrictionEnzymes.empty()) {
    QMessageBox::information(this, "Restriction Enzymes", "No restriction enzymes loaded in database.");
    return;
  }
  
  QDialog dialog(this);
  dialog.setWindowTitle("Select Restriction Enzymes to Display");
  dialog.resize(520, 600);
  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  
  QHBoxLayout *searchLayout = new QHBoxLayout();
  searchLayout->addWidget(new QLabel("🔍 Filter:"));
  QLineEdit *filterEdit = new QLineEdit(&dialog);
  filterEdit->setPlaceholderText("Type enzyme name or alias (e.g. EcoRI, PsiI, BamHI)...");
  filterEdit->setClearButtonEnabled(true);
  searchLayout->addWidget(filterEdit);
  layout->addLayout(searchLayout);
  
  QStandardItemModel model(this);
  for (size_t i = 0; i < restrictionEnzymes.size(); ++i) {
    const auto &enz = restrictionEnzymes[i];
    QString itemText = QString("%1  [%2]").arg(enz.name, -22).arg(enz.motif);
    QStandardItem *item = new QStandardItem(itemText);
    item->setCheckable(true);
    item->setCheckState(enz.enabled ? Qt::Checked : Qt::Unchecked);
    item->setData(static_cast<int>(i), Qt::UserRole);
    model.appendRow(item);
  }
  
  QSortFilterProxyModel proxy(this);
  proxy.setSourceModel(&model);
  proxy.setFilterCaseSensitivity(Qt::CaseInsensitive);
  
  connect(filterEdit, &QLineEdit::textChanged, &proxy, [&](const QString &txt) {
    proxy.setFilterRegularExpression(QRegularExpression(QRegularExpression::escape(txt), QRegularExpression::CaseInsensitiveOption));
  });
  
  QListView listView(&dialog);
  listView.setModel(&proxy);
  listView.setFont(QFont("Monospace", 9));
  layout->addWidget(&listView);
  
  QLabel *lblCount = new QLabel(&dialog);
  auto updateCounter = [&]() {
    int active = 0;
    for (int r = 0; r < model.rowCount(); ++r) {
      if (model.item(r)->checkState() == Qt::Checked) active++;
    }
    lblCount->setText(QString("Selected: <b>%1</b> / %2 enzymes").arg(active).arg(model.rowCount()));
  };
  updateCounter();
  connect(&model, &QStandardItemModel::itemChanged, updateCounter);
  layout->addWidget(lblCount);
  
  QHBoxLayout *actionBtns = new QHBoxLayout();
  QPushButton *btnSelectAll = new QPushButton("Check All");
  QPushButton *btnDeselectAll = new QPushButton("Uncheck All");
  QPushButton *btnInvert = new QPushButton("Invert");
  
  connect(btnSelectAll, &QPushButton::clicked, [&]() {
    for (int r = 0; r < model.rowCount(); ++r) model.item(r)->setCheckState(Qt::Checked);
  });
  connect(btnDeselectAll, &QPushButton::clicked, [&]() {
    for (int r = 0; r < model.rowCount(); ++r) model.item(r)->setCheckState(Qt::Unchecked);
  });
  connect(btnInvert, &QPushButton::clicked, [&]() {
    for (int r = 0; r < model.rowCount(); ++r) {
      auto *item = model.item(r);
      item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
    }
  });
  
  actionBtns->addWidget(btnSelectAll);
  actionBtns->addWidget(btnDeselectAll);
  actionBtns->addWidget(btnInvert);
  layout->addLayout(actionBtns);
  
  QHBoxLayout *dlgBtns = new QHBoxLayout();
  QPushButton *btnOk = new QPushButton("Apply");
  QPushButton *btnCancel = new QPushButton("Cancel");
  btnOk->setDefault(true);
  dlgBtns->addStretch();
  dlgBtns->addWidget(btnCancel);
  dlgBtns->addWidget(btnOk);
  layout->addLayout(dlgBtns);
  
  connect(btnCancel, &QPushButton::clicked, &dialog, &QDialog::reject);
  connect(btnOk, &QPushButton::clicked, &dialog, &QDialog::accept);
  
  if (dialog.exec() == QDialog::Accepted) {
    QStringList disabledList;
    for (int r = 0; r < model.rowCount(); ++r) {
      QStandardItem *item = model.item(r);
      int origIdx = item->data(Qt::UserRole).toInt();
      if (origIdx >= 0 && origIdx < static_cast<int>(restrictionEnzymes.size())) {
        bool isEnabled = (item->checkState() == Qt::Checked);
        restrictionEnzymes[origIdx].enabled = isEnabled;
        if (!isEnabled) disabledList.append(restrictionEnzymes[origIdx].name);
      }
    }
    QSettings settings("ChromaZ", "ChromaZ");
    settings.setValue("disabledEnzymes", disabledList);
    update();
  }
}

bool ContigWidget::matchIupac(QChar seqChar, QChar motifChar) const {
  seqChar = seqChar.toUpper();
  motifChar = motifChar.toUpper();
  if (motifChar == seqChar || motifChar == 'N') return true;
  if (motifChar == 'R') return (seqChar == 'A' || seqChar == 'G');
  if (motifChar == 'Y') return (seqChar == 'C' || seqChar == 'T');
  if (motifChar == 'S') return (seqChar == 'G' || seqChar == 'C');
  if (motifChar == 'W') return (seqChar == 'A' || seqChar == 'T');
  if (motifChar == 'K') return (seqChar == 'G' || seqChar == 'T');
  if (motifChar == 'M') return (seqChar == 'A' || seqChar == 'C');
  if (motifChar == 'B') return (seqChar == 'C' || seqChar == 'G' || seqChar == 'T');
  if (motifChar == 'D') return (seqChar == 'A' || seqChar == 'G' || seqChar == 'T');
  if (motifChar == 'H') return (seqChar == 'A' || seqChar == 'C' || seqChar == 'T');
  if (motifChar == 'V') return (seqChar == 'A' || seqChar == 'C' || seqChar == 'G');
  return false;
}

QString ContigWidget::getSelectedSequence() const {
  if (selTrack == SEL_NONE || selStartCol < 0 || selEndCol < 0) return {};

  const int minC = std::min(selStartCol, selEndCol);
  const int len = std::abs(selEndCol - selStartCol) + 1;
  QString seq;

  if (selTrack == SEL_REF) {
    seq = referenceSeq.mid(minC, len);
  } else if (selTrack == SEL_CONSENSUS) {
    seq = consensusSeq.mid(minC, len);
  } else if (selTrack <= SEL_ORF1 && selTrack >= SEL_ORF_REV3) {
    const int frame = SEL_ORF1 - selTrack;
    seq = getOrfLine(frame, referenceSeq).mid(minC, len);
  } else if (selTrack <= SEL_ORF_B1 && selTrack >= SEL_ORF_BREV3) {
    const int frame = SEL_ORF_B1 - selTrack;
    seq = getOrfLine(frame, consensusSeq).mid(minC, len);
  } else if (selTrack >= 0 && selTrack < static_cast<int>(tracks.size())) {
    const auto &track = tracks[selTrack];
    const QString &full = track.alignment.alignedRead.isEmpty()
                            ? track.originalData.sequence
                            : track.alignment.alignedRead;
    seq = full.mid(minC, len);
  }

  seq.remove('-').remove(' ');
  return seq;
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
  state.showOrfA = showOrfA;
  state.showOrfB = showOrfB;
  state.isAutoReference = isAutoReference;
  state.currentAlgorithm = currentAlgorithm;
  state.minIdentityThreshold = minIdentityThreshold;
  undoStack.push_back(state);
  if (undoStack.size() > 100) undoStack.erase(undoStack.begin());
  redoStack.clear();
  markModified();
}

void ContigWidget::undo() {
  abortAlignment();
  if (undoStack.empty()) return;
  WidgetState currentState;
  currentState.referenceSeq = referenceSeq; currentState.consensusSeq = consensusSeq;
  currentState.tracks = tracks; currentState.selTrack = selTrack;
  currentState.selStartCol = selStartCol; currentState.selEndCol = selEndCol;
  currentState.autoRCEnabled = autoRCEnabled; currentState.showOrfA = showOrfA; currentState.showOrfB = showOrfB; currentState.isAutoReference = isAutoReference;
  currentState.currentAlgorithm = currentAlgorithm; currentState.minIdentityThreshold = minIdentityThreshold;
  redoStack.push_back(currentState);
  WidgetState prevState = undoStack.back(); undoStack.pop_back();
  referenceSeq = prevState.referenceSeq; consensusSeq = prevState.consensusSeq;
  tracks = prevState.tracks; selTrack = prevState.selTrack;
  selStartCol = prevState.selStartCol; selEndCol = prevState.selEndCol;
  autoRCEnabled = prevState.autoRCEnabled; showOrfA = prevState.showOrfA; showOrfB = prevState.showOrfB; isAutoReference = prevState.isAutoReference;
  currentAlgorithm = prevState.currentAlgorithm; minIdentityThreshold = prevState.minIdentityThreshold;
  emit autoRCChanged(autoRCEnabled);
  emit showOrfAChanged(showOrfA); emit showOrfBChanged(showOrfB);
  emit alignmentAlgorithmChanged(static_cast<int>(currentAlgorithm));
  emit minIdentityThresholdChanged(minIdentityThreshold);
  calculateConsensusAndSNPs(); updateLayoutGeometry(); fitToWindowHeight(); update(); markModified();
}

void ContigWidget::redo() {
  abortAlignment();
  if (redoStack.empty()) return;
  WidgetState currentState;
  currentState.referenceSeq = referenceSeq; currentState.consensusSeq = consensusSeq;
  currentState.tracks = tracks; currentState.selTrack = selTrack;
  currentState.selStartCol = selStartCol; currentState.selEndCol = selEndCol;
  currentState.autoRCEnabled = autoRCEnabled; currentState.showOrfA = showOrfA; currentState.showOrfB = showOrfB; currentState.isAutoReference = isAutoReference;
  currentState.currentAlgorithm = currentAlgorithm; currentState.minIdentityThreshold = minIdentityThreshold;
  undoStack.push_back(currentState);
  WidgetState nextState = redoStack.back(); redoStack.pop_back();
  referenceSeq = nextState.referenceSeq; consensusSeq = nextState.consensusSeq;
  tracks = nextState.tracks; selTrack = nextState.selTrack;
  selStartCol = nextState.selStartCol; selEndCol = nextState.selEndCol;
  autoRCEnabled = nextState.autoRCEnabled; showOrfA = nextState.showOrfA; showOrfB = nextState.showOrfB; isAutoReference = nextState.isAutoReference;
  currentAlgorithm = nextState.currentAlgorithm; minIdentityThreshold = nextState.minIdentityThreshold;
  emit autoRCChanged(autoRCEnabled);
  emit showOrfAChanged(showOrfA); emit showOrfBChanged(showOrfB);
  emit alignmentAlgorithmChanged(static_cast<int>(currentAlgorithm));
  emit minIdentityThresholdChanged(minIdentityThreshold);
  calculateConsensusAndSNPs(); updateLayoutGeometry(); fitToWindowHeight(); update(); markModified();
}

QChar ContigWidget::translateCodon(const QString& codon, int tableIndex) const {
  if (codon.length() != 3 || codon.contains('-') || codon.contains('N') || codon.contains(' ')) return ' ';
  QString c = codon.toUpper();
  if (c=="TTT" || c=="TTC") return 'F';
  if (c=="TTA" || c=="TTG" || c=="CTT" || c=="CTC" || c=="CTA" || c=="CTG") return 'L';
  if (c=="ATT" || c=="ATC") return 'I';
  if (c=="ATA") return (tableIndex == 1) ? 'M' : 'I';
  if (c=="ATG") return 'M';
  if (c=="GTT" || c=="GTC" || c=="GTA" || c=="GTG") return 'V';
  if (c=="TCT" || c=="TCC" || c=="TCA" || c=="TCG" || c=="AGT" || c=="AGC") return 'S';
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
  if (c=="AGA" || c=="AGG") return (tableIndex == 1) ? '*' : 'R';
  if (c=="GGT" || c=="GGC" || c=="GGA" || c=="GGG") return 'G';
  return ' ';
}

QString ContigWidget::getOrfLine(int frame, const QString &targetSeq) const {
  bool isReverse = (frame >= 3);
  int shift = frame % 3;
  QString workingSeq = isReverse ? Ab1Parser::reverseComplement(targetSeq) : targetSeq;
  QString result(targetSeq.length(), ' ');
  std::vector<int> seqMap;
  QString ungapped = "";
  for (int col = 0; col < workingSeq.length(); ++col) {
    if (workingSeq[col] != '-' && workingSeq[col] != ' ') {
      ungapped += workingSeq[col];
      seqMap.push_back(col);
    }
  }
  for (int i = shift; i + 2 < ungapped.length(); i += 3) {
    QString codon = ungapped.mid(i, 3);
    QChar aa = translateCodon(codon, translationTable);
    if (aa != ' ') {
      int centerCol = seqMap[i + 1];
      if (isReverse) centerCol = targetSeq.length() - 1 - centerCol;
      if (centerCol >= 0 && centerCol < result.length()) result[centerCol] = aa;
    }
  }
  return result;
}

int ContigWidget::calculateHeaderHeight() const {
  int h = 30;
  if (showOrfA) h += 85;
  h += 30;
  if (showOrfB) h += 85;
  return h;
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
  int curY = calculateHeaderHeight() + 10;
  int numIn = getNumInContig();
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
  abortAlignment();
  pushUndoState();
  AlignedTrack track;
  track.filePath = filePath;
  track.fileName = QFileInfo(filePath).fileName();
  track.originalData = data;
  // Гэпы из FASTA/TXT не относятся к риду: убираем их, чтобы индексы исходной последовательности
  // совпадали с основаниями в выравнивании (у хроматограмм и FASTQ '-' в последовательности нет).
  if (track.originalData.qualityScores.empty() && track.originalData.basePositions.empty())
    track.originalData.sequence.remove('-');
  tracks.push_back(track);
  fitToWindowHeight();
  updateLayoutGeometry();
  update();
  return true;
}

void ContigWidget::setAutoRC(bool enable) {
  if (autoRCEnabled == enable) return;
  if (!tracks.empty() || !referenceSeq.isEmpty() || !consensusSeq.isEmpty())
    pushUndoState();
  autoRCEnabled = enable;
  suppressUndo = true; runAlignment(); suppressUndo = false;
}

void ContigWidget::setShowOrfA(bool enable) {
  if (showOrfA == enable) return;
  if (!tracks.empty() || !referenceSeq.isEmpty() || !consensusSeq.isEmpty())
    pushUndoState();
  showOrfA = enable;
  emit showOrfAChanged(showOrfA);
  updateLayoutGeometry();
  fitToWindowHeight();
  update();
}

void ContigWidget::setShowOrfB(bool enable) {
  if (showOrfB == enable) return;
  if (!tracks.empty() || !referenceSeq.isEmpty() || !consensusSeq.isEmpty())
    pushUndoState();
  showOrfB = enable;
  emit showOrfBChanged(showOrfB);
  updateLayoutGeometry();
  fitToWindowHeight();
  update();
}

void ContigWidget::setAlignmentAlgorithm(int algoIdx) {
  if (algoIdx < 0 || algoIdx > 7) return;
  const auto algorithm = static_cast<AlignmentAlgorithm>(algoIdx);
  const bool changed = (currentAlgorithm != algorithm);

  // Save the previous settings and alignment before a user-requested re-alignment.
  if (!tracks.empty() || !referenceSeq.isEmpty() || !consensusSeq.isEmpty())
    pushUndoState();
  if (changed) {
    currentAlgorithm = algorithm;
  }

  suppressUndo = true;
  runAlignment();
  suppressUndo = false;
  if (changed) emit alignmentAlgorithmChanged(algoIdx);
}

void ContigWidget::setTranslationTable(int tableIdx) {
  const int boundedIndex = std::clamp(tableIdx, 0, 2);
  if (translationTable == boundedIndex) return;
  translationTable = boundedIndex;
  emit translationTableChanged(translationTable);
  update();
}

void ContigWidget::setMinIdentityThreshold(double val) {
  if (!std::isfinite(val)) return;
  val = std::clamp(val, 0.0, 100.0);
  if (std::abs(minIdentityThreshold - val) < 1e-9) return;

  if (!tracks.empty() || !referenceSeq.isEmpty() || !consensusSeq.isEmpty())
    pushUndoState();
  minIdentityThreshold = val;
  QString selectedTrackPath;
  if (selTrack >= 0 && selTrack < static_cast<int>(tracks.size()))
    selectedTrackPath = tracks[static_cast<size_t>(selTrack)].filePath;

  for (auto &track : tracks) track.inContig = (track.alignment.identity >= minIdentityThreshold);
  std::stable_sort(tracks.begin(), tracks.end(), [](const AlignedTrack &a, const AlignedTrack &b) {
    return a.inContig > b.inContig;
  });
  if (!selectedTrackPath.isEmpty()) {
    const auto selected = std::find_if(tracks.begin(), tracks.end(), [&](const AlignedTrack &track) {
      return track.filePath == selectedTrackPath;
    });
    if (selected != tracks.end()) selTrack = static_cast<int>(std::distance(tracks.begin(), selected));
    else { selTrack = SEL_NONE; selStartCol = selEndCol = -1; }
  }
  searchMatches.clear();
  currentMatchIdx = -1;
  emit minIdentityThresholdChanged(minIdentityThreshold);
  calculateConsensusAndSNPs();
  fitToWindowHeight();
  updateLayoutGeometry();
  update();
}

void ContigWidget::setConsensusIupac(bool enable) {
  if (useIupacConsensus == enable) return;
  useIupacConsensus = enable;
  emit consensusIupacChanged(useIupacConsensus);
  calculateConsensusAndSNPs();
  update();
}

void ContigWidget::toggleRestrictionSites(bool enable) {
  showRestrictionSites = enable;
  emit restrictionSitesToggled(showRestrictionSites);
  update();
}

void ContigWidget::readSelectionAloud() {
  QString seq = getSelectedSequence();
  if (seq.isEmpty()) return;
  QString spokenText;
  for (QChar c : seq) { spokenText += c; spokenText += ", "; }
  if (tts) { tts->stop(); tts->say(spokenText); }
}

void ContigWidget::stopReading() {
  if (tts) tts->stop();
}

void ContigWidget::setSpeechRate(double rate) {
  speechRate = std::clamp(rate, -1.0, 1.0);
  if (tts) tts->setRate(speechRate);
  emit speechRateChanged(speechRate);
}

void ContigWidget::openInNcbiBlast() {
  QString seq = getSelectedSequence();
  if (seq.isEmpty()) return;
  QUrl url("https://blast.ncbi.nlm.nih.gov/Blast.cgi");
  QUrlQuery query;
  query.addQueryItem("PROGRAM", "blastn");
  query.addQueryItem("PAGE_TYPE", "BlastSearch");
  query.addQueryItem("QUERY", seq);
  url.setQuery(query);
  QDesktopServices::openUrl(url);
}

void ContigWidget::showSequenceProperties() {
  QString seq = getSelectedSequence();
  if (seq.isEmpty()) return;
  
  QDialog dialog(this);
  dialog.setWindowTitle("Sequence & Protein Properties");
  dialog.resize(800, 650);
  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  QTabWidget *tabs = new QTabWidget();
  
  QWidget *nucTab = new QWidget();
  QVBoxLayout *nl = new QVBoxLayout(nucTab);
  int gcCount = seq.count('G', Qt::CaseInsensitive) + seq.count('C', Qt::CaseInsensitive);
  int atCount = seq.count('A', Qt::CaseInsensitive) + seq.count('T', Qt::CaseInsensitive);
  int totalLen = seq.length();
  double gcPercent = (totalLen > 0) ? (gcCount * 100.0 / totalLen) : 0.0;
  double atPercent = (totalLen > 0) ? (atCount * 100.0 / totalLen) : 0.0;
  
  const std::string seqStd = seq.toStdString();
  const double tmWallace = TmCalc::wallace(seqStd);
  const double tmGc = TmCalc::gcFormula(seqStd);

  QLabel *lblNucSummary = new QLabel(
    QString("<b>Length:</b> %1 bp &nbsp;&nbsp;|&nbsp;&nbsp; "
            "<b>GC Content:</b> %2% &nbsp;&nbsp;|&nbsp;&nbsp; "
            "<b>AT Content:</b> %3%")
    .arg(totalLen).arg(gcPercent, 0, 'f', 1).arg(atPercent, 0, 'f', 1));
  lblNucSummary->setTextFormat(Qt::RichText);
  nl->addWidget(lblNucSummary);

  QLabel *lblTm = new QLabel(
    QString("<b>Tm, Wallace rule:</b> %1 °C <span style='color:gray'>(2·(A+T) + 4·(G+C); intended for &lt; 14 nt)</span><br>"
            "<b>Tm, GC formula:</b> %2 °C <span style='color:gray'>(64.9 + 41·(G+C − 16.4)/N; intended for ≥ 14 nt)</span>")
    .arg(tmWallace, 0, 'f', 1).arg(tmGc, 0, 'f', 1));
  lblTm->setTextFormat(Qt::RichText);
  nl->addWidget(lblTm);
              
              nl->addWidget(new QLabel("<b>Nucleotide Sequence (with 1-based coordinates):</b>"));
              QPlainTextEdit *txtNucSeq = new QPlainTextEdit();
              txtNucSeq->setReadOnly(true);
              txtNucSeq->setFont(QFont("Monospace", 9));
              txtNucSeq->setPlainText(formatSequenceWithCoordinates(seq, 60, 10));
              txtNucSeq->setMaximumHeight(140);
              nl->addWidget(txtNucSeq);

              // Уточнённая Tm по методу ближайших соседей — после ввода концентраций соли и праймера.
              QGroupBox *nnBox = new QGroupBox("Nearest-neighbor Tm (SantaLucia 1998): enter salt and primer concentrations");
              QFormLayout *nnForm = new QFormLayout(nnBox);
              auto makeSpin = [](double maxV, int decimals, const QString &suffix) {
                QDoubleSpinBox *sp = new QDoubleSpinBox();
                sp->setRange(0.0, maxV);
                sp->setDecimals(decimals);
                sp->setSuffix(suffix);
                sp->setSpecialValueText("—");   // 0 = не задано
                sp->setValue(0.0);
                return sp;
              };
              QDoubleSpinBox *spNa = makeSpin(2000.0, 1, " mM");
              QDoubleSpinBox *spMg = makeSpin(100.0, 2, " mM");
              QDoubleSpinBox *spPrimer = makeSpin(100000.0, 1, " nM");
              nnForm->addRow("Na⁺ / K⁺ concentration:", spNa);
              nnForm->addRow("Mg²⁺ concentration (optional):", spMg);
              nnForm->addRow("Primer concentration:", spPrimer);
              QLabel *lblNN = new QLabel("Enter Na⁺ (or Mg²⁺) and primer concentrations to calculate.");
              lblNN->setTextFormat(Qt::RichText);
              lblNN->setWordWrap(true);
              nnForm->addRow(lblNN);
              auto updateNN = [spNa, spMg, spPrimer, lblNN, seqStd]() {
                const double na = spNa->value(), mg = spMg->value(), pr = spPrimer->value();
                if ((na <= 0.0 && mg <= 0.0) || pr <= 0.0) {
                  lblNN->setText("Enter Na⁺ (or Mg²⁺) and primer concentrations to calculate.");
                  return;
                }
                TmCalc::NNResult r = TmCalc::nearestNeighbor(seqStd, na, mg, pr);
                if (!r.valid) {
                  lblNN->setText("<span style='color:#a00'>Not available: " + QString::fromStdString(r.reason) + "</span>");
                  return;
                }
                QString notes;
                if (r.selfComplementary) notes += "<br><i>Self-complementary sequence (symmetry correction applied).</i>";
                if (seqStd.size() > 60) notes += "<br><i>The nearest-neighbor model is reliable for oligonucleotides up to about 60 nt.</i>";
                lblNN->setText(QString("<b>Tm (nearest-neighbor): %1 °C</b><br>"
                                       "ΔH = %2 kcal/mol, ΔS = %3 cal/(K·mol) (salt-corrected), ΔG(37 °C) = %4 kcal/mol<br>"
                                       "Monovalent-equivalent cation concentration: %5 mM%6")
                               .arg(r.tm, 0, 'f', 1).arg(r.deltaH, 0, 'f', 1).arg(r.deltaS, 0, 'f', 1)
                               .arg(r.deltaG37, 0, 'f', 1).arg(r.naEquivalentMM, 0, 'f', 1).arg(notes));
              };
              connect(spNa, &QDoubleSpinBox::valueChanged, nnBox, [updateNN](double) { updateNN(); });
              connect(spMg, &QDoubleSpinBox::valueChanged, nnBox, [updateNN](double) { updateNN(); });
              connect(spPrimer, &QDoubleSpinBox::valueChanged, nnBox, [updateNN](double) { updateNN(); });
              nl->addWidget(nnBox);
              
              int winGC = std::min(15, totalLen);
              if (winGC > 1 && winGC % 2 == 0) winGC--;
              int halfGC = winGC / 2;
              QVector<double> gcValues;
              for (int i = 0; i < totalLen; ++i) {
                int start = std::max(0, i - halfGC);
                int end = std::min(totalLen - 1, i + halfGC);
                int localGC = 0;
                for (int j = start; j <= end; ++j) {
                  QChar c = seq[j].toUpper();
                  if (c == 'G' || c == 'C' || c == 'S') localGC++;
                }
                gcValues.push_back(double(localGC) / (end - start + 1) * 100.0);
              }
              
              nl->addWidget(new QLabel(QString("<b>GC Content Profile</b> (Sliding window: %1 bp, baseline: 50%):").arg(winGC)));
              
              QScrollArea *saNuc = new QScrollArea();
              saNuc->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
              saNuc->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
              saNuc->setFixedHeight(190);
              
              SequenceGraphWidget *gcGraph = new SequenceGraphWidget(QColor(33, 150, 243));
              gcGraph->values = gcValues;
              gcGraph->sequence = seq;
              gcGraph->minV = 0.0;
              gcGraph->maxV = 100.0;
              gcGraph->baselineVal = 50.0;
              gcGraph->showBaseline = true;
              gcGraph->topLabel = "100% GC";
              gcGraph->bottomLabel = "0% GC";
              gcGraph->updateDimensions(740);
              
              saNuc->setWidget(gcGraph);
              nl->addWidget(saNuc);
              tabs->addTab(nucTab, "🧬 Nucleotide Properties");
              
              QWidget *protTab = new QWidget();
              QVBoxLayout *pl = new QVBoxLayout(protTab);
              QString aaSeq;
              for (int i = 0; i + 2 < seq.length(); i += 3) {
                QChar aa = translateCodon(seq.mid(i, 3), translationTable);
                if (aa != ' ' && aa != '*') aaSeq += aa;
              }
              
              double totalMW = aaSeq.isEmpty() ? 0.0 : 18.015;
              for (QChar aa : aaSeq) totalMW += getAAMolecularWeight(aa);
              
              QLabel *lblProtSummary = new QLabel(
                QString("<b>Translated Length:</b> %1 aa &nbsp;&nbsp;|&nbsp;&nbsp; "
                          "<b>Molecular Weight:</b> %2 Da (~%3 kDa)")
                .arg(aaSeq.length())
                .arg(totalMW, 0, 'f', 2)
                .arg(totalMW / 1000.0, 0, 'f', 2));
                          lblProtSummary->setTextFormat(Qt::RichText);
                          pl->addWidget(lblProtSummary);
                          
                          pl->addWidget(new QLabel("<b>Translated Protein Sequence (Frame +1, with coordinates):</b>"));
                          QPlainTextEdit *txtProtSeq = new QPlainTextEdit();
                          txtProtSeq->setReadOnly(true);
                          txtProtSeq->setFont(QFont("Monospace", 9));
                          txtProtSeq->setPlainText(formatSequenceWithCoordinates(aaSeq, 50, 10));
                          txtProtSeq->setMaximumHeight(130);
                          pl->addWidget(txtProtSeq);
                          
                          int winKD = std::min(7, (int)aaSeq.length());
                          if (winKD > 1 && winKD % 2 == 0) winKD--;
                          int halfKD = winKD / 2;
                          QVector<double> kdValues;
                          for (int i = 0; i < aaSeq.length(); ++i) {
                            int start = std::max(0, i - halfKD);
                            int end = std::min((int)aaSeq.length() - 1, i + halfKD);
                            double sum = 0.0;
                            for (int j = start; j <= end; ++j) {
                              sum += getKyteDoolittle(aaSeq[j]);
                            }
                            kdValues.push_back(sum / (end - start + 1));
                          }
                          
                          pl->addWidget(new QLabel(QString("<b>Kyte-Doolittle Hydrophobicity Profile</b> (Window: %1 aa, neutral: 0.0):").arg(winKD)));
                          
                          QScrollArea *saProt = new QScrollArea();
                          saProt->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                          saProt->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
                          saProt->setFixedHeight(190);
                          
                          SequenceGraphWidget *kdGraph = new SequenceGraphWidget(QColor(233, 30, 99));
                          kdGraph->values = kdValues;
                          kdGraph->sequence = aaSeq;
                          kdGraph->minV = -4.5;
                          kdGraph->maxV = 4.5;
                          kdGraph->baselineVal = 0.0;
                          kdGraph->showBaseline = true;
                          kdGraph->topLabel = "▲ Hydrophobic";
                          kdGraph->bottomLabel = "▼ Hydrophilic";
                          kdGraph->updateDimensions(740);
                          
                          saProt->setWidget(kdGraph);
                          pl->addWidget(saProt);
                          tabs->addTab(protTab, "🧪 Protein Properties (Frame +1)");
                          
                          layout->addWidget(tabs);
                          dialog.exec();
}

bool ContigWidget::exportConsensusGenBank(const QString &filePath) {
  QString ungapped;
  for (int i = 0; i < consensusSeq.length(); ++i) {
    if (consensusSeq[i] != '-' && consensusSeq[i] != ' ') ungapped += consensusSeq[i].toUpper();
  }
  if (ungapped.isEmpty()) return false;   // нечего экспортировать — не создаём пустой файл

  QSaveFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
  QTextStream out(&file);
  
  // Дата в формате GenBank (DD-MON-YYYY) всегда на английском, независимо от языка системы.
  QString date = QLocale::c().toString(QDate::currentDate(), "dd-MMM-yyyy").toUpper();
  out << QString("LOCUS       ChromaZ_Consensus   %1 bp    DNA     linear   UNC %2\n").arg(ungapped.length(), -6).arg(date);
  out << "FEATURES             Location/Qualifiers\n";
  out << QString("     source          1..%1\n").arg(ungapped.length());
  
  if (!restrictionEnzymes.empty()) {
    for (const auto& enz : restrictionEnzymes) {
      if (!enz.enabled) continue; // экспортируем только включенные
      int mLen = enz.motif.length();
      if (ungapped.length() < mLen) continue;
      for (int pos = 0; pos <= ungapped.length() - mLen; ++pos) {
        bool match = true;
        for (int j = 0; j < mLen; ++j) {
          if (!matchIupac(ungapped[pos + j], enz.motif[j])) { match = false; break; }
        }
        if (match) {
          out << QString("     misc_feature    %1..%2\n").arg(pos + 1).arg(pos + mLen);
          out << QString("                     /note=\"%1 restriction site\"\n").arg(enz.name);
        }
      }
    }
  }
  
  out << "ORIGIN\n";
  for (int i = 0; i < ungapped.length(); i += 60) {
    out << QString("%1").arg(i + 1, 9, 10, QChar(' ')) << " ";
    for (int j = 0; j < 60 && i + j < ungapped.length(); j += 10) {
      out << ungapped.mid(i + j, 10).toLower() << " ";
    }
    out << "\n";
  }
  out << "//\n";
  out.flush();
  return file.commit();
}

void ContigWidget::exportConsensusGenBankPrompt() {
  QString filePath = QFileDialog::getSaveFileName(this, "Export GenBank File", "ChromaZ_Consensus.gbk", "GenBank Files (*.gb *.gbk);;All Files (*)");
  if (!filePath.isEmpty() && !exportConsensusGenBank(filePath)) {
    QMessageBox::warning(this, "Export GenBank", "Nothing was exported: there is no consensus sequence or the file could not be written:\n" + filePath);
  }
}

void ContigWidget::showPrintExportDialog() {
  QDialog dialog(this);
  dialog.setWindowTitle("Print & Export Alignment Report");
  dialog.resize(520, 420);
  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  
  QGroupBox *grpDest = new QGroupBox("Output Target");
  QVBoxLayout *destLayout = new QVBoxLayout(grpDest);
  QRadioButton *rbPdf = new QRadioButton("Save as PDF Document (*.pdf)");
  QRadioButton *rbPrinter = new QRadioButton("Send to System / Physical Printer");
  rbPdf->setChecked(true);
  destLayout->addWidget(rbPdf);
  
  QHBoxLayout *pdfPathLayout = new QHBoxLayout();
  QLineEdit *txtPdfPath = new QLineEdit("ChromaZ_Alignment.pdf");
  QPushButton *btnBrowse = new QPushButton("Browse...");
  pdfPathLayout->addWidget(txtPdfPath);
  pdfPathLayout->addWidget(btnBrowse);
  destLayout->addLayout(pdfPathLayout);
  destLayout->addWidget(rbPrinter);
  layout->addWidget(grpDest);
  
  connect(btnBrowse, &QPushButton::clicked, &dialog, [this, txtPdfPath]() {
    QString p = QFileDialog::getSaveFileName(this, "Export PDF Report", txtPdfPath->text(), "PDF Files (*.pdf)");
    if (!p.isEmpty()) txtPdfPath->setText(p);
  });
  connect(rbPdf, &QRadioButton::toggled, txtPdfPath, &QWidget::setEnabled);
  connect(rbPdf, &QRadioButton::toggled, btnBrowse, &QWidget::setEnabled);
  
  QGroupBox *grpOpts = new QGroupBox("Included Content");
  QVBoxLayout *optsLayout = new QVBoxLayout(grpOpts);
  QCheckBox *cbTraces = new QCheckBox("Print chromatogram peak curves (Traces)");
  QCheckBox *cbQuality = new QCheckBox("Print Phred quality bars");
  QCheckBox *cbOrf = new QCheckBox("Print translated amino acid frames (ORF A & B)");
  QCheckBox *cbRef = new QCheckBox("Include Reference sequence");
  QCheckBox *cbCons = new QCheckBox("Include Consensus sequence");
  
  cbTraces->setChecked(true);
  cbQuality->setChecked(true);
  cbOrf->setChecked(showOrfA || showOrfB);
  cbRef->setChecked(true);
  cbCons->setChecked(true);
  
  optsLayout->addWidget(cbTraces);
  optsLayout->addWidget(cbQuality);
  optsLayout->addWidget(cbOrf);
  optsLayout->addWidget(cbRef);
  optsLayout->addWidget(cbCons);
  
  QHBoxLayout *bpLayout = new QHBoxLayout();
  bpLayout->addWidget(new QLabel("Bases per line / block:"));
  QSpinBox *spinBases = new QSpinBox();
  spinBases->setRange(30, 200);
  spinBases->setValue(60);
  bpLayout->addWidget(spinBases);
  bpLayout->addStretch();
  optsLayout->addLayout(bpLayout);
  
  QHBoxLayout *presetLayout = new QHBoxLayout();
  QPushButton *btnFull = new QPushButton("Full Report (Traces + ORFs)");
  QPushButton *btnCompact = new QPushButton("Sequences Only (Compact)");
  presetLayout->addWidget(btnFull);
  presetLayout->addWidget(btnCompact);
  optsLayout->addLayout(presetLayout);
  
  layout->addWidget(grpOpts);
  
  connect(btnFull, &QPushButton::clicked, [&]() {
    cbTraces->setChecked(true);
    cbQuality->setChecked(true);
    cbOrf->setChecked(true);
  });
  connect(btnCompact, &QPushButton::clicked, [&]() {
    cbTraces->setChecked(false);
    cbQuality->setChecked(false);
    cbOrf->setChecked(false);
  });
  
  QHBoxLayout *btnBox = new QHBoxLayout();
  QPushButton *btnOk = new QPushButton("Print / Export");
  QPushButton *btnCancel = new QPushButton("Cancel");
  btnOk->setDefault(true);
  btnBox->addStretch();
  btnBox->addWidget(btnCancel);
  btnBox->addWidget(btnOk);
  layout->addLayout(btnBox);
  
  connect(btnCancel, &QPushButton::clicked, &dialog, &QDialog::reject);
  connect(btnOk, &QPushButton::clicked, &dialog, &QDialog::accept);
  
  if (dialog.exec() != QDialog::Accepted) return;
  
  PrintExportOptions opt;
  opt.basesPerLine = spinBases->value();
  opt.includeTraces = cbTraces->isChecked();
  opt.includeQuality = cbQuality->isChecked();
  opt.includeOrfA = cbOrf->isChecked() && showOrfA;
  opt.includeOrfB = cbOrf->isChecked() && showOrfB;
  opt.includeRef = cbRef->isChecked();
  opt.includeConsensus = cbCons->isChecked();
  
  if (rbPdf->isChecked()) {
    opt.outputType = PrintExportOptions::PdfFile;
    opt.pdfPath = txtPdfPath->text();
    QPdfWriter writer(opt.pdfPath);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageOrientation(QPageLayout::Portrait);
    writer.setResolution(300);
    renderAlignmentToDevice(&writer, opt);
    QMessageBox::information(this, "Export Completed", "PDF report successfully generated:\n" + opt.pdfPath);
  } else {
    opt.outputType = PrintExportOptions::SystemPrinter;
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog pDialog(&printer, this);
    if (pDialog.exec() == QDialog::Accepted) {
      renderAlignmentToDevice(&printer, opt);
    }
  }
}

void ContigWidget::renderAlignmentToDevice(QPagedPaintDevice *device, const PrintExportOptions &opt) {
  int maxCols = getMaxCols();
  if (maxCols == 0) return;
  
  QPainter painter(device);
  painter.setRenderHint(QPainter::Antialiasing);
  
  int dpi = device->logicalDpiX();
  if (dpi <= 0) dpi = 300;
  double s = double(dpi) / 96.0;
  
  QFont blockHeaderFont("SansSerif", 9, QFont::Bold);
  QFont labelFont("SansSerif", 7, QFont::Bold);
  QFont seqFont("Monospace", 8, QFont::Bold);
  QFont orfFont("Monospace", 6);
  QFont coordFont("Monospace", 6);
  
  QFontMetrics fmTitle(blockHeaderFont, device);
  QFontMetrics fmLabel(labelFont, device);
  QFontMetrics fmSeq(seqFont, device);
  QFontMetrics fmOrf(orfFont, device);
  QFontMetrics fmCoord(coordFont, device);
  
  int hTitle = fmTitle.lineSpacing() + int(3 * s);
  int hSeq = fmSeq.lineSpacing() + int(2 * s);
  int hOrf = fmOrf.lineSpacing() + int(1 * s);
  int hCoord = fmCoord.lineSpacing() + int(1 * s);
  int hBar = opt.includeQuality ? int(5 * s) : 0;
  int hTrace = opt.includeTraces ? int(55 * s) : 0;
  
  QRect pageRect = device->pageLayout().paintRectPixels(dpi);
  int pageLeft = pageRect.left() + int(15 * s);
  int pageRight = pageRect.right() - int(15 * s);
  int pageTop = pageRect.top() + int(15 * s);
  int pageBottom = pageRect.bottom() - int(15 * s);
  int pageWidth = pageRight - pageLeft;
  
  int leftMarginWidth = int(140 * s);
  int plotWidth = pageWidth - leftMarginWidth;
  int bpl = std::max(10, opt.basesPerLine);
  double baseStepX = double(plotWidth) / bpl;
  
  int singleTrackH = hCoord + hSeq + (opt.includeQuality ? (hBar + int(2 * s)) : 0) +
    (opt.includeTraces ? (hTrace + int(4 * s)) : 0) + int(8 * s);
  
  int currentY = pageTop;
  int totalBlocks = (maxCols + bpl - 1) / bpl;
  
  auto ensureVerticalSpace = [&](int neededHeight) {
    if (currentY + neededHeight > pageBottom) {
      device->newPage();
      currentY = pageTop;
    }
  };
  
  for (int b = 0; b < totalBlocks; ++b) {
    int startCol = b * bpl;
    int endCol = std::min(maxCols, (b + 1) * bpl);
    
    ensureVerticalSpace(hTitle + hSeq * 2 + int(10 * s));
    
    painter.setFont(blockHeaderFont);
    painter.setPen(Qt::black);
    painter.drawText(pageLeft, currentY + fmTitle.ascent(),
                     QString("Block %1 (Bases %2 - %3)").arg(b + 1).arg(startCol + 1).arg(endCol));
    currentY += hTitle;
    
    if (opt.includeRef && !referenceSeq.isEmpty()) {
      ensureVerticalSpace(hCoord + hSeq + (opt.includeOrfA ? 6 * hOrf : 0));
      
      painter.setFont(labelFont);
      painter.setPen(Qt::darkGreen);
      painter.drawText(pageLeft, currentY + hCoord + fmLabel.ascent(), isAutoReference ? "CONTIG:" : "REF:");
      
      painter.setFont(coordFont);
      painter.setPen(Qt::gray);
      for (int col = startCol; col < endCol && col < referenceSeq.length(); ++col) {
        int pos = col + 1;
        if (pos % 10 == 0 || pos == 1) {
          double x = pageLeft + leftMarginWidth + (col - startCol) * baseStepX;
          painter.drawText(x, currentY + fmCoord.ascent(), QString::number(pos));
        }
      }
      currentY += hCoord;
      
      painter.setFont(seqFont);
      for (int col = startCol; col < endCol && col < referenceSeq.length(); ++col) {
        double x = pageLeft + leftMarginWidth + (col - startCol) * baseStepX;
        QChar base = referenceSeq[col].toUpper();
        if (base == 'A') painter.setPen(Qt::green);
        else if (base == 'C') painter.setPen(Qt::blue);
        else if (base == 'G') painter.setPen(Qt::black);
        else if (base == 'T') painter.setPen(Qt::red);
        else painter.setPen(Qt::gray);
        if (base != '-' && base != ' ') {
          painter.drawText(x, currentY + fmSeq.ascent(), QString(base));
        }
      }
      currentY += hSeq;
      
      if (opt.includeOrfA) {
        painter.setFont(orfFont);
        for (int frame = 0; frame < 6; ++frame) {
          QString orfStr = getOrfLine(frame, referenceSeq);
          for (int col = startCol; col < endCol && col < orfStr.length(); ++col) {
            QChar aa = orfStr[col];
            if (aa != ' ') {
              double x = pageLeft + leftMarginWidth + (col - startCol) * baseStepX;
              if (aa == 'M') painter.setPen(Qt::darkGreen);
              else if (aa == '*') painter.setPen(Qt::red);
              else if (frame >= 3) painter.setPen(QColor(230, 81, 0));
              else painter.setPen(Qt::darkBlue);
              painter.drawText(x, currentY + fmOrf.ascent(), QString(aa));
            }
          }
          currentY += hOrf;
        }
      }
    }
    
    if (opt.includeConsensus && !consensusSeq.isEmpty()) {
      ensureVerticalSpace(hCoord + hSeq + (opt.includeOrfB ? 6 * hOrf : 0));
      
      painter.setFont(labelFont);
      painter.setPen(QColor(156, 39, 176));
      painter.drawText(pageLeft, currentY + hCoord + fmLabel.ascent(), "CONSENSUS:");
      
      painter.setFont(coordFont);
      painter.setPen(Qt::gray);
      for (int col = startCol; col < endCol && col < consensusSeq.length(); ++col) {
        int pos = col + 1;
        if (pos % 10 == 0 || pos == 1) {
          double x = pageLeft + leftMarginWidth + (col - startCol) * baseStepX;
          painter.drawText(x, currentY + fmCoord.ascent(), QString::number(pos));
        }
      }
      currentY += hCoord;
      
      painter.setFont(seqFont);
      for (int col = startCol; col < endCol && col < consensusSeq.length(); ++col) {
        double x = pageLeft + leftMarginWidth + (col - startCol) * baseStepX;
        QChar base = consensusSeq[col].toUpper();
        if (base == 'A') painter.setPen(Qt::green);
        else if (base == 'C') painter.setPen(Qt::blue);
        else if (base == 'G') painter.setPen(Qt::black);
        else if (base == 'T') painter.setPen(Qt::red);
        else if (base == '-') painter.setPen(Qt::gray);
        else painter.setPen(QColor(156, 39, 176));
        if (base != '-' && base != ' ') {
          painter.drawText(x, currentY + fmSeq.ascent(), QString(base));
        }
      }
      currentY += hSeq;
      
      if (opt.includeOrfB) {
        painter.setFont(orfFont);
        for (int frame = 0; frame < 6; ++frame) {
          QString orfStr = getOrfLine(frame, consensusSeq);
          for (int col = startCol; col < endCol && col < orfStr.length(); ++col) {
            QChar aa = orfStr[col];
            if (aa != ' ') {
              double x = pageLeft + leftMarginWidth + (col - startCol) * baseStepX;
              if (aa == 'M') painter.setPen(Qt::darkGreen);
              else if (aa == '*') painter.setPen(Qt::red);
              else if (frame >= 3) painter.setPen(QColor(230, 81, 0));
              else painter.setPen(Qt::darkBlue);
              painter.drawText(x, currentY + fmOrf.ascent(), QString(aa));
            }
          }
          currentY += hOrf;
        }
      }
    }
    
    currentY += int(6 * s);
    
    for (size_t t = 0; t < tracks.size(); ++t) {
      ensureVerticalSpace(singleTrackH);
      auto &track = tracks[t];
      Ab1Data dispData = Ab1Parser::getOrientedData(track.originalData, track.alignment.isReverseComplement);
      const QString &readAligned = track.alignment.alignedRead.isEmpty() ? dispData.sequence : track.alignment.alignedRead;
      
      painter.setFont(labelFont);
      painter.setPen(track.inContig ? Qt::darkBlue : Qt::red);
      QString rcMark = track.alignment.isReverseComplement ? " [RC]" : "";
      QRect nameRect(pageLeft, currentY, leftMarginWidth - int(8 * s), singleTrackH);
      painter.drawText(nameRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, track.fileName + rcMark);
      
      int rawBaseIdx = 0;
      for (int col = 0; col < startCol && col < readAligned.length(); ++col) {
        if (readAligned[col] != '-') rawBaseIdx++;
      }
      
      painter.setFont(coordFont);
      painter.setPen(Qt::gray);
      int tempRawIdx = 0;
      for (int col = 0; col < startCol && col < readAligned.length(); ++col) {
        if (readAligned[col] != '-' && readAligned[col] != ' ') tempRawIdx++;
      }
      for (int col = startCol; col < endCol && col < readAligned.length(); ++col) {
        if (readAligned[col] != '-' && readAligned[col] != ' ') {
          tempRawIdx++;
          if (tempRawIdx % 10 == 0 || tempRawIdx == 1) {
            double x = pageLeft + leftMarginWidth + (col - startCol) * baseStepX;
            painter.drawText(x, currentY + fmCoord.ascent(), QString::number(tempRawIdx));
          }
        }
      }
      currentY += hCoord;
      
      const int blockRawStart = rawBaseIdx;
      painter.setFont(seqFont);
      for (int col = startCol; col < endCol && col < readAligned.length(); ++col) {
        double x = pageLeft + leftMarginWidth + (col - startCol) * baseStepX;
        QChar base = readAligned[col];
        QChar refC = (col < referenceSeq.length()) ? referenceSeq[col].toUpper() : '?';
        
        bool isMismatch = (base.isLetter() && base != '-' && base != ' ' && refC.isLetter() && refC != '?' && refC != '-' && base.toUpper() != refC);
        if (isMismatch) {
          painter.fillRect(x - 1, currentY, baseStepX, hSeq, QColor(229, 57, 53));
          painter.setPen(Qt::white);
        } else {
          if (base == 'A') painter.setPen(Qt::green);
          else if (base == 'C') painter.setPen(Qt::blue);
          else if (base == 'G') painter.setPen(Qt::black);
          else if (base == 'T') painter.setPen(Qt::red);
          else painter.setPen(Qt::gray);
        }
        if (base != '-' && base != ' ') {
          painter.drawText(x, currentY + fmSeq.ascent(), QString(base));
        }
        
        if (opt.includeQuality && base != '-' && rawBaseIdx < (int)dispData.qualityScores.size()) {
          int q = dispData.qualityScores[rawBaseIdx];
          QColor qCol = (q >= 30) ? QColor(76, 175, 80) : ((q >= 20) ? QColor(255, 193, 7) : QColor(244, 67, 54));
          painter.fillRect(x, currentY + hSeq + 1, std::max(1.0, baseStepX - 1), hBar, qCol);
        }
        if (base != '-') rawBaseIdx++;
      }
      currentY += hSeq + (opt.includeQuality ? (hBar + int(2 * s)) : 0);
      
      if (opt.includeTraces && !dispData.basePositions.empty()) {
        struct BasePt { double x; int col; };
        std::vector<BasePt> pts;
        for (int col = startCol; col < endCol && col < readAligned.length(); ++col) {
          if (readAligned[col] != '-') {
            pts.push_back({pageLeft + leftMarginWidth + (col - startCol) * baseStepX, col});
          }
        }
        
        int baseline = currentY + hTrace;
        int maxVal = 1200;
        double yFactor = (hTrace * 0.85) / maxVal;
        
        auto drawPrintTrace = [&](const std::vector<int> &trace, QColor col) {
          if (trace.empty() || pts.size() < 2) return;
          QPainterPath path;
          bool started = false;
          for (size_t bp = 0; bp + 1 < pts.size() && size_t(blockRawStart) + bp + 1 < dispData.basePositions.size(); ++bp) {
            int sStart = dispData.basePositions[blockRawStart + bp];
            int sEnd = dispData.basePositions[blockRawStart + bp + 1];
            if (sStart >= (int)trace.size()) continue;
            sEnd = std::min(sEnd, (int)trace.size() - 1);
            if (sStart >= sEnd) continue;
            
            for (int sample = sStart; sample <= sEnd; ++sample) {
              double t = double(sample - sStart) / (sEnd - sStart);
              double xPos = pts[bp].x + t * (pts[bp + 1].x - pts[bp].x);
              double yPos = baseline - std::min(trace[sample], maxVal) * yFactor;
              if (!started) { path.moveTo(xPos, yPos); started = true; }
              else path.lineTo(xPos, yPos);
            }
          }
          painter.setPen(QPen(col, std::max(1.0, 1.0 * s)));
          painter.drawPath(path);
        };
        
        drawPrintTrace(dispData.traceA, Qt::green);
        drawPrintTrace(dispData.traceC, Qt::blue);
        drawPrintTrace(dispData.traceG, Qt::black);
        drawPrintTrace(dispData.traceT, Qt::red);
        
        currentY += hTrace + int(4 * s);
      }
      
      currentY += int(8 * s);
    }
    
    currentY += int(15 * s);
  }
}

void ContigWidget::applyTrimToTrack(AlignedTrack &tr, int trimLeftCount, int trimRightCount) {
  auto &q = tr.originalData.qualityScores;
  auto &seq = tr.originalData.sequence;
  auto &ploc = tr.originalData.basePositions;
  auto &tA = tr.originalData.traceA;
  auto &tC = tr.originalData.traceC;
  auto &tG = tr.originalData.traceG;
  auto &tT = tr.originalData.traceT;

  int n = seq.length();
  if (n == 0) return;

  int start = trimLeftCount;
  int end = n - 1 - trimRightCount;

  if (start > end || start >= n) {
    seq.clear(); q.clear(); ploc.clear(); tA.clear(); tC.clear(); tG.clear(); tT.clear();
    return;
  }

  int newLen = end - start + 1;
  int traceLen = 0;
  if (!tA.empty()) traceLen = std::min({(int)tA.size(), (int)tC.size(), (int)tG.size(), (int)tT.size()});

  int startSample = 0, endSample = traceLen;
  if (traceLen > 0 && !ploc.empty()) {
    if (start > 0 && start < (int)ploc.size()) startSample = (ploc[start - 1] + ploc[start]) / 2;
    if (end + 1 < (int)ploc.size()) endSample = (ploc[end] + ploc[end + 1]) / 2;
    startSample = std::clamp(startSample, 0, traceLen);
    endSample = std::clamp(endSample, startSample, traceLen);
  }

  seq = seq.mid(start, newLen);

  if (start < (int)q.size()) {
    int newQLen = std::min(newLen, (int)q.size() - start);
    q = std::vector<int>(q.begin() + start, q.begin() + start + newQLen);
  } else { q.clear(); }

  bool traceSliced = false;
  if (traceLen > 0 && startSample < endSample) {
    tA = std::vector<int>(tA.begin() + startSample, tA.begin() + endSample);
    tC = std::vector<int>(tC.begin() + startSample, tC.begin() + endSample);
    tG = std::vector<int>(tG.begin() + startSample, tG.begin() + endSample);
    tT = std::vector<int>(tT.begin() + startSample, tT.begin() + endSample);
    traceSliced = true;
  }

  if (start < (int)ploc.size()) {
    int newPLen = std::min(newLen, (int)ploc.size() - start);
    ploc = std::vector<int>(ploc.begin() + start, ploc.begin() + start + newPLen);
    if (traceSliced) for (auto &p : ploc) { p -= startSample; if (p < 0) p = 0; }
  } else { ploc.clear(); }
}

void ContigWidget::trimLowQualityEnds(int minPhred) {
  abortAlignment();
  pushUndoState();
  for (auto &tr : tracks) {
    auto &q = tr.originalData.qualityScores;
    int n = tr.originalData.sequence.length();
    if (n == 0 || q.empty()) continue;

    int start = 0;
    while (start < n && start < (int)q.size() && q[start] < minPhred) start++;

    int end = n - 1;
    while (end >= start && end < (int)q.size() && q[end] < minPhred) end--;

    int trimLeft = start;
    int trimRight = n - 1 - end;
    if (trimLeft > 0 || trimRight > 0) applyTrimToTrack(tr, trimLeft, trimRight);
  }
  suppressUndo = true; runAlignment(); suppressUndo = false;
}

int ContigWidget::findFuzzyMatch(const QString& text, const QString& pattern, int maxMismatches) {
  if (pattern.isEmpty() || text.length() < pattern.length()) return -1;
  for (int i = 0; i <= text.length() - pattern.length(); ++i) {
    int mismatches = 0;
    for (int j = 0; j < pattern.length(); ++j) {
      if (text[i+j].toUpper() != pattern[j].toUpper()) mismatches++;
      if (mismatches > maxMismatches) break;
    }
    if (mismatches <= maxMismatches) return i;
  }
  return -1;
}

void ContigWidget::trimVectors() {
  abortAlignment();
  pushUndoState();
  struct VectorSeq { QString name; QString seq; };
  const std::vector<VectorSeq> VECTORS = {
    {"M13F", "GTAAAACGACGGCCAGT"}, {"M13R", "CAGGAAACAGCTATGAC"},
     {"T7", "TAATACGACTCACTATAGGG"}, {"SP6", "ATTTAGGTGACACTATAG"}
  };
  
  for (auto &tr : tracks) {
    QString seq = tr.originalData.sequence;
    if (seq.length() < 20) continue;
    int trimLeft = 0, trimRight = 0, searchWindow = std::min(150, (int)seq.length());
    QString head = seq.left(searchWindow), tail = seq.right(searchWindow);
    
    for (const auto& v : VECTORS) {
      int allowance = v.seq.length() / 8;
      int idx = findFuzzyMatch(head, v.seq, allowance);
      if (idx != -1) trimLeft = std::max(trimLeft, idx + (int)v.seq.length());
      
      QString rcVec = Ab1Parser::reverseComplement(v.seq);
      idx = findFuzzyMatch(head, rcVec, allowance);
      if (idx != -1) trimLeft = std::max(trimLeft, idx + (int)rcVec.length());
      
      idx = findFuzzyMatch(tail, v.seq, allowance);
      if (idx != -1) trimRight = std::max(trimRight, (int)tail.length() - idx);
      
      idx = findFuzzyMatch(tail, rcVec, allowance);
      if (idx != -1) trimRight = std::max(trimRight, (int)tail.length() - idx);
    }
    
    if (trimLeft > 0 || trimRight > 0) {
      if (trimLeft + trimRight >= seq.length()) { trimLeft = seq.length(); trimRight = 0; }
      applyTrimToTrack(tr, trimLeft, trimRight);
    }
  }
  suppressUndo = true; runAlignment(); suppressUndo = false;
}

bool ContigWidget::saveProject(const QString &filePath, int compressionLevel) {
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
  root["showOrfA"] = showOrfA;
  root["showOrfB"] = showOrfB;
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
    
    QJsonObject algObj = trObj["alignment"].toObject();
    algObj["alignedRef"] = tr.alignment.alignedRef;
    algObj["alignedRead"] = tr.alignment.alignedRead;
    algObj["score"] = tr.alignment.score;
    algObj["identity"] = tr.alignment.identity;
    algObj["isReverseComplement"] = tr.alignment.isReverseComplement;
    trObj["alignment"] = algObj;
    
    tracksArray.append(trObj);
  }
  root["tracks"] = tracksArray;

  // QSaveFile пишет во временный файл и подменяет целевой только при успехе: сбой посреди записи
  // не повредит уже существующий проект.
  QSaveFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) return false;

  const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Compact);
  const QByteArray packed = ProjectIO::pack(json, std::clamp(compressionLevel, 0, 9));
  if (file.write(packed) != packed.size()) { file.cancelWriting(); return false; }
  // Флаг "изменено" здесь не сбрасывается: так сохраняется и автосохранение сессии.
  // Сбрасывает его MainWindow после явного сохранения пользователем.
  return file.commit();
}

bool ContigWidget::loadProject(const QString &filePath) {
  abortAlignment();
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) return false;

  // Новый формат — JSON, сжатый qCompress; старые сохранения — обычный JSON.
  const QByteArray json = ProjectIO::unpack(file.readAll());
  if (json.isEmpty()) return false;

  QJsonParseError parseError;
  const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
  if (parseError.error != QJsonParseError::NoError || !doc.isObject()) return false;

  const QJsonObject root = doc.object();
  if (root.value("format").toString() != "ChromaZ" || !root.value("tracks").isArray())
    return false;

  // Validate the project before clearing the currently open workspace.
  const QJsonArray tracksArray = root.value("tracks").toArray();
  for (const QJsonValue &value : tracksArray) {
    if (!value.isObject()) return false;
    const QJsonObject trackObject = value.toObject();
    if (!trackObject.value("originalData").isObject() ||
        !trackObject.value("originalData").toObject().value("sequence").isString())
      return false;
  }

  clearAll();
  referenceSeq = root.value("referenceSeq").toString();
  consensusSeq = root.value("consensusSeq").toString();
  isAutoReference = root.value("isAutoReference").toBool();

  const double savedThreshold = root.value("minIdentityThreshold").toDouble(45.0);
  minIdentityThreshold = std::isfinite(savedThreshold)
                           ? std::clamp(savedThreshold, 0.0, 100.0) : 45.0;
  useIupacConsensus = root.value("useIupacConsensus").toBool(false);
  translationTable = std::clamp(root.value("translationTable").toInt(0), 0, 2);
  const int algoIndex = root.value("currentAlgorithm").toInt(0);
  currentAlgorithm = static_cast<AlignmentAlgorithm>(std::clamp(algoIndex, 0, 7));
  autoRCEnabled = root.value("autoRCEnabled").toBool(true);
  showOrfA = root.value("showOrfA").toBool(true);
  showOrfB = root.value("showOrfB").toBool(true);

  const double savedScaleX = root.value("scaleX").toDouble(14.0);
  const double savedScaleY = root.value("scaleY").toDouble(0.05);
  scaleX = std::isfinite(savedScaleX) ? std::clamp(savedScaleX, 2.0, 100.0) : 14.0;
  scaleY = std::isfinite(savedScaleY) ? std::clamp(savedScaleY, 0.001, 10.0) : 0.05;
  trackHeight = std::clamp(root.value("trackHeight").toInt(120), 60, 1000);
  leftMarginWidth = std::clamp(root.value("leftMarginWidth").toInt(220), 80, 800);

  auto jsonToVec = [](const QJsonArray &array) {
    std::vector<int> result;
    result.reserve(static_cast<size_t>(array.size()));
    for (const QJsonValue &item : array) result.push_back(item.toInt());
    return result;
  };

  for (const QJsonValue &value : tracksArray) {
    const QJsonObject trackObject = value.toObject();
    AlignedTrack track;
    track.filePath = trackObject.value("filePath").toString();
    track.fileName = trackObject.value("fileName").toString();
    track.inContig = trackObject.value("inContig").toBool(true);
    track.overrideRC = trackObject.value("overrideRC").toBool(false);

    const QJsonObject ab1Object = trackObject.value("originalData").toObject();
    track.originalData.sequence = ab1Object.value("sequence").toString();
    track.originalData.isValid = ab1Object.value("isValid").toBool(true);
    track.originalData.traceA = jsonToVec(ab1Object.value("traceA").toArray());
    track.originalData.traceC = jsonToVec(ab1Object.value("traceC").toArray());
    track.originalData.traceG = jsonToVec(ab1Object.value("traceG").toArray());
    track.originalData.traceT = jsonToVec(ab1Object.value("traceT").toArray());
    track.originalData.basePositions = jsonToVec(ab1Object.value("basePositions").toArray());
    track.originalData.qualityScores = jsonToVec(ab1Object.value("qualityScores").toArray());

    const QJsonObject alignmentObject = trackObject.value("alignment").toObject();
    track.alignment.alignedRef = alignmentObject.value("alignedRef").toString();
    track.alignment.alignedRead = alignmentObject.value("alignedRead").toString();
    track.alignment.score = alignmentObject.value("score").toInt();
    track.alignment.identity = alignmentObject.value("identity").toDouble();
    track.alignment.isReverseComplement = alignmentObject.value("isReverseComplement").toBool();
    tracks.push_back(std::move(track));
  }

  calculateConsensusAndSNPs();
  fitToWindowHeight();
  updateLayoutGeometry();
  update();
  undoStack.clear();
  redoStack.clear();
  emit autoRCChanged(autoRCEnabled);
  emit showOrfAChanged(showOrfA);
  emit showOrfBChanged(showOrfB);
  emit consensusIupacChanged(useIupacConsensus);
  emit alignmentAlgorithmChanged(static_cast<int>(currentAlgorithm));
  emit translationTableChanged(translationTable);
  emit minIdentityThresholdChanged(minIdentityThreshold);
  setModified(false);
  emit projectLoaded(filePath);
  return true;
}

void ContigWidget::findSequence(const QString &query, bool forward) {
  QString qClean = query.trimmed().toUpper();
  if (qClean.isEmpty()) {
    searchMatches.clear(); currentMatchIdx = -1; lastSearchQuery.clear(); selTrack = SEL_NONE; update(); return;
  }
  if (qClean != lastSearchQuery || searchMatches.empty()) {
    lastSearchQuery = qClean; searchMatches.clear(); currentMatchIdx = -1;
    int qLen = qClean.length(), pos = 0;
    const QString refU = referenceSeq.toUpper(), consU = consensusSeq.toUpper();
    while ((pos = refU.indexOf(qClean, pos)) != -1) { searchMatches.push_back({SEL_REF, pos, qLen}); pos++; }
    pos = 0;
    while ((pos = consU.indexOf(qClean, pos)) != -1) { searchMatches.push_back({SEL_CONSENSUS, pos, qLen}); pos++; }
    for (size_t t = 0; t < tracks.size(); ++t) {
      const QString &read = tracks[t].alignment.alignedRead.isEmpty() ? tracks[t].originalData.sequence : tracks[t].alignment.alignedRead;
      pos = 0; const QString readU = read.toUpper();
      while ((pos = readU.indexOf(qClean, pos)) != -1) { searchMatches.push_back({(int)t, pos, qLen}); pos++; }
    }
  }
  if (searchMatches.empty()) { currentMatchIdx = -1; selTrack = SEL_NONE; update(); return; }
  
  if (forward) currentMatchIdx = (currentMatchIdx + 1) % searchMatches.size();
  else currentMatchIdx = (currentMatchIdx - 1 + searchMatches.size()) % searchMatches.size();
  
  auto match = searchMatches[currentMatchIdx];
  selTrack = match.trackId; selStartCol = match.startCol; selEndCol = match.startCol + match.length - 1;
  
  const QWidget *w = this;
  while (w) {
    const QScrollArea *sa = qobject_cast<const QScrollArea*>(w);
    if (sa) {
      if (sa->horizontalScrollBar()) sa->horizontalScrollBar()->setValue(leftMarginWidth + match.startCol * scaleX - sa->viewport()->width() / 2);
      if (sa->verticalScrollBar()) {
        int targetY = 0;
        if (match.trackId == SEL_REF) targetY = 10;
        else if (match.trackId == SEL_CONSENSUS) targetY = 80;
        else if (match.trackId >= 0 && match.trackId < (int)tracks.size()) targetY = getTrackY(match.trackId);
        sa->verticalScrollBar()->setValue(targetY - sa->viewport()->height() / 2);
      }
      break;
    }
    w = w->parentWidget();
  }
  update();
}

void ContigWidget::calculateConsensusAndSNPs() {
  consensusSeq.clear(); snpColumns.clear(); currentSnpIdx = -1;
  int maxCols = getMaxCols();
  
  for (int col = 0; col < maxCols; ++col) {
    int countA = 0, countC = 0, countG = 0, countT = 0, active = 0;
    std::map<QChar, int> baseFreq;
    
    for (const auto &tr : tracks) {
      if (!tr.inContig) continue;
      const QString &read = tr.alignment.alignedRead.isEmpty() ? tr.originalData.sequence : tr.alignment.alignedRead;
      if (col < read.length()) {
        QChar b = read[col].toUpper();
        if (b != '-' && b != 'N' && b != ' ' && b.isLetter()) {
          active++;
          if (b == 'A') countA++; else if (b == 'C') countC++; else if (b == 'G') countG++; else if (b == 'T') countT++;
          baseFreq[b]++;
        }
      }
    }
    
    if (active == 0) { consensusSeq += '-'; continue; }
    
    int maxCount = std::max({countA, countC, countG, countT});
    int topCount = (countA == maxCount) + (countC == maxCount) + (countG == maxCount) + (countT == maxCount);
    
    if (!useIupacConsensus) {
      if (topCount == 1) {
        if (countA == maxCount) consensusSeq += 'A'; else if (countC == maxCount) consensusSeq += 'C'; else if (countG == maxCount) consensusSeq += 'G'; else if (countT == maxCount) consensusSeq += 'T';
      } else { consensusSeq += 'N'; }
    } else {
      if (topCount == 1) {
        if (countA == maxCount) consensusSeq += 'A'; else if (countC == maxCount) consensusSeq += 'C'; else if (countG == maxCount) consensusSeq += 'G'; else if (countT == maxCount) consensusSeq += 'T';
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
    if (active >= 2 && baseFreq.size() > 1) snpColumns.push_back(col);
  }
  computeColumnStats();
}

void ContigWidget::jumpNextSNP() {
  if (snpColumns.empty()) return;
  currentSnpIdx = (currentSnpIdx + 1) % snpColumns.size();
  int col = snpColumns[currentSnpIdx];
  selTrack = SEL_CONSENSUS;
  selStartCol = col;
  selEndCol = col;
  const QWidget *w = this;
  while (w) {
    const QScrollArea *sa = qobject_cast<const QScrollArea*>(w);
    if (sa && sa->horizontalScrollBar()) { sa->horizontalScrollBar()->setValue(leftMarginWidth + col * scaleX - sa->viewport()->width() / 2); break; }
    w = w->parentWidget();
  }
  update();
}

void ContigWidget::jumpPrevSNP() {
  if (snpColumns.empty()) return;
  currentSnpIdx = (currentSnpIdx - 1 + snpColumns.size()) % snpColumns.size();
  int col = snpColumns[currentSnpIdx];
  selTrack = SEL_CONSENSUS;
  selStartCol = col;
  selEndCol = col;
  const QWidget *w = this;
  while (w) {
    const QScrollArea *sa = qobject_cast<const QScrollArea*>(w);
    if (sa && sa->horizontalScrollBar()) { sa->horizontalScrollBar()->setValue(leftMarginWidth + col * scaleX - sa->viewport()->width() / 2); break; }
    w = w->parentWidget();
  }
  update();
}

bool ContigWidget::exportConsensusFasta(const QString &filePath) {
  QString seq = consensusSeq;
  seq.remove('-').remove(' ');          // консенсус без колонок-гэпов
  if (seq.isEmpty()) return false;
  QSaveFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
  QTextStream out(&file);
  out << ">ChromaZ_Consensus\n";
  for (int i = 0; i < seq.length(); i += 70) out << seq.mid(i, 70) << "\n";
  out.flush();
  return file.commit();
}

bool ContigWidget::exportViewImage(const QString &filePath) {
  QImage img(size(), QImage::Format_ARGB32);
  if (img.isNull()) return false;       // слишком большое изображение (подробности покажет вызывающий код)
  img.fill(Qt::white);
  { QPainter p(&img); render(&p); }
  return img.save(filePath);
}

void ContigWidget::fitToWindowHeight(int viewportHeight) {
  if (tracks.empty()) return;
  if (viewportHeight <= 0) {
    const QWidget *w = this;
    while (w) {
      const QScrollArea *sa = qobject_cast<const QScrollArea*>(w);
      if (sa && sa->viewport()) { viewportHeight = sa->viewport()->height(); break; }
      w = w->parentWidget();
    }
  }
  if (viewportHeight <= 80) return;
  int numOut = tracks.size() - getNumInContig();
  int extraHeader = calculateHeaderHeight() + (numOut > 0 ? 35 : 0) + 20;
  int availableForTracks = viewportHeight - extraHeader;
  if (availableForTracks > 0) {
    trackHeight = std::max(60, availableForTracks / static_cast<int>(tracks.size()));
    updateLayoutGeometry(); update();
  }
}

void ContigWidget::updateLayoutGeometry() {
  int maxCols = getMaxCols();
  int reqWidth = leftMarginWidth + maxCols * scaleX + 150;
  int reqHeight = getTrackY(tracks.size()) + 60;
  setMinimumSize(reqWidth, reqHeight);
  resize(reqWidth, reqHeight);
}

void ContigWidget::setReference(const QString &seq, bool realign) {
  QString clean = seq.toUpper();
  clean.remove('-').remove(' ').remove('\n').remove('\r');
  if (clean.isEmpty()) return;
  abortAlignment();   // иначе результат прерванного задания перезаписал бы новый референс
  pushUndoState();
  referenceSeq = clean;
  isAutoReference = false;
  updateLayoutGeometry();
  update();
  if (realign && !tracks.empty()) { suppressUndo = true; runAlignment(); suppressUndo = false; }
}

void ContigWidget::clearAll() {
  abortAlignment();
  pushUndoState();
  referenceSeq.clear();
  consensusSeq.clear();
  tracks.clear();
  isAutoReference = false;
  snpColumns.clear();
  currentSnpIdx = -1;
  selTrack = SEL_NONE;
  selStartCol = -1;
  selEndCol = -1;
  isSelecting = false;
  searchMatches.clear();
  currentMatchIdx = -1;
  lastSearchQuery.clear();
  updateLayoutGeometry();
  update();
}

void ContigWidget::runAlignment() {
  if (tracks.empty()) return;
  if (worker) {                 // уже идёт: отменяем и перезапускаем с актуальными данными
    pendingRerun = true;
    worker->cancel();
    return;
  }
  startAlignmentJob(!suppressUndo);
}

void ContigWidget::startAlignmentJob(bool pushUndo, bool rerun) {
  if (!rerun) jobPushedUndo = pushUndo;
  if (pushUndo) pushUndoState();

  AlignJobInput in;
  in.algo = currentAlgorithm;
  in.autoRC = autoRCEnabled;
  in.refineIterations = refineIterations;
  in.threads = threadCount;
  in.minIdentity = minIdentityThreshold;
  if (!isAutoReference) {
    QString r = referenceSeq;
    r.remove('-').remove(' ');
    in.reference = r;   // пустой референс = автоматический
  }
  for (const auto &tr : tracks) {
    AlignJobTrackIn t;
    t.rawSeq = tr.originalData.sequence;
    t.fixedOrientation = !autoRCEnabled || tr.overrideRC;
    t.fixedRC = tr.alignment.isReverseComplement;
    in.tracks.push_back(t);
  }

  jobTrackCount = static_cast<int>(tracks.size());
  const quint64 gen = ++jobGeneration;
  worker = new AlignWorker(std::move(in), this);
  connect(worker, &AlignWorker::progress, this, [this, gen](int done, int total, const QString &stage) {
    if (gen == jobGeneration) emit alignmentProgress(done, total, stage);
  });
  connect(worker, &QThread::finished, this, [this, gen]() {
    if (gen == jobGeneration && worker) onAlignmentFinished();
  });
  emit alignmentStarted();
  worker->start();
}

void ContigWidget::onAlignmentFinished() {
  AlignWorker *w = worker;
  if (!w) return;
  w->wait();
  worker = nullptr;
  AlignJobOutput out = std::move(w->result);
  w->deleteLater();

  const bool rerun = pendingRerun;
  pendingRerun = false;
  if (rerun || static_cast<int>(tracks.size()) != jobTrackCount) {
    if (!tracks.empty()) { startAlignmentJob(false, true); return; }
    emit alignmentFinished(false);
    return;
  }

  bool applied = false;
  if (!out.cancelled && !out.reference.isEmpty()) {
    applyAlignmentResult(out);
    applied = true;
  } else if (out.cancelled && jobPushedUndo && !undoStack.empty()) {
    undoStack.pop_back();     // пользователь отменил — снимок "до выравнивания" не нужен
  }
  emit alignmentFinished(applied);
}

void ContigWidget::applyAlignmentResult(AlignJobOutput &out) {
  dragSourceTrackIdx = -2;   // порядок ридов ниже может измениться — прерываем возможное перетаскивание
  QString selectedTrackPath;
  if (selTrack >= 0 && selTrack < static_cast<int>(tracks.size()))
    selectedTrackPath = tracks[static_cast<size_t>(selTrack)].filePath;

  MasterAlignment master = buildMaster(out.reference, out.tracks);
  referenceSeq = master.masterRef;
  isAutoReference = out.autoReference;

  for (size_t k = 0; k < tracks.size() && k < out.tracks.size(); ++k) {
    AlignedTrack &tr = tracks[k];
    const AlignJobTrackOut &r = out.tracks[k];
    if (!r.valid) { tr.alignment.alignedRead.clear(); continue; }
    tr.alignment.alignedRef = master.masterRef;
    tr.alignment.alignedRead = master.masterReads[k];
    tr.alignment.isReverseComplement = r.isRC;
    tr.alignment.identity = r.identity;
    tr.alignment.score = r.score;
    tr.inContig = (r.identity >= minIdentityThreshold);
  }

  std::stable_sort(tracks.begin(), tracks.end(), [](const AlignedTrack &a, const AlignedTrack &b) { return a.inContig > b.inContig; });

  if (!selectedTrackPath.isEmpty()) {
    const auto it = std::find_if(tracks.begin(), tracks.end(), [&](const AlignedTrack &t) { return t.filePath == selectedTrackPath; });
    if (it != tracks.end()) selTrack = static_cast<int>(std::distance(tracks.begin(), it));
    else { selTrack = SEL_NONE; selStartCol = selEndCol = -1; }
  }

  calculateConsensusAndSNPs();
  searchMatches.clear();
  currentMatchIdx = -1;
  fitToWindowHeight();
  updateLayoutGeometry();
  update();
  markModified();
}

void ContigWidget::cancelAlignment() {
  if (!worker) return;
  pendingRerun = false;
  worker->cancel();            // onAlignmentFinished() завершит очистку
}

// Синхронная остановка: нужна перед операциями, меняющими набор ридов.
void ContigWidget::abortAlignment() {
  if (!worker) return;
  pendingRerun = false;
  ++jobGeneration;             // сигналы старого задания игнорируются
  worker->cancel();
  worker->wait();
  delete worker;
  worker = nullptr;
  if (jobPushedUndo && !undoStack.empty()) undoStack.pop_back();
  emit alignmentFinished(false);
}

void ContigWidget::setRefineIterations(int n) {
  n = std::clamp(n, 0, 10);
  if (n == refineIterations) return;
  refineIterations = n;
  if (!tracks.empty()) { suppressUndo = true; runAlignment(); suppressUndo = false; }
}

void ContigWidget::setThreadCount(int n) { threadCount = std::clamp(n, 0, 256); }

void ContigWidget::markModified() {
  if (!modifiedFlag) { modifiedFlag = true; emit modifiedChanged(true); }
}

void ContigWidget::setModified(bool m) {
  if (modifiedFlag == m) return;
  modifiedFlag = m;
  emit modifiedChanged(m);
}

void ContigWidget::computeColumnStats() {
  ColumnStats st;
  int cols = 0;
  for (const auto &tr : tracks) if (tr.inContig) cols = std::max(cols, static_cast<int>(tr.alignment.alignedRead.length()));
  st.totalCols = cols;
  st.depth.assign(cols, 0);
  st.meanQ.assign(cols, -1.0f);
  std::vector<double> sumQ(cols, 0.0);
  std::vector<int> cntQ(cols, 0);

  for (const auto &tr : tracks) {
    if (!tr.inContig) continue;
    const QString &read = tr.alignment.alignedRead;
    if (read.isEmpty()) continue;
    const auto &q = tr.originalData.qualityScores;
    const int qn = static_cast<int>(q.size());
    const bool rc = tr.alignment.isReverseComplement;
    int raw = 0;
    for (int c = 0; c < read.length(); ++c) {
      const QChar ch = read[c];
      if (ch == '-') continue;
      if (ch != ' ' && ch.isLetter()) {
        st.depth[c]++;
        const int qi = rc ? qn - 1 - raw : raw;
        if (qi >= 0 && qi < qn) { sumQ[c] += q[static_cast<size_t>(qi)]; cntQ[c]++; }
      }
      ++raw;   // удалённое основание (' ') тоже занимает позицию в исходном риде
    }
  }

  long long depthSum = 0;
  for (int c = 0; c < cols; ++c) {
    if (cntQ[c] > 0) st.meanQ[c] = static_cast<float>(sumQ[c] / cntQ[c]);
    if (st.depth[c] > 0) {
      st.coveredCols++;
      depthSum += st.depth[c];
      if (st.depth[c] >= 2) st.multiCols++;
      if (st.meanQ[c] >= 0 && st.meanQ[c] < 20.0f) st.lowQualCols++;
    }
    st.maxDepth = std::max(st.maxDepth, st.depth[c]);
  }
  st.meanDepth = st.coveredCols > 0 ? static_cast<double>(depthSum) / st.coveredCols : 0.0;
  stats = std::move(st);
  emit statsChanged();
}

void ContigWidget::centerOnColumn(int col) {
  const QWidget *w = this;
  while (w) {
    const QScrollArea *sa = qobject_cast<const QScrollArea*>(w);
    if (sa) {
      if (sa->horizontalScrollBar())
        sa->horizontalScrollBar()->setValue(static_cast<int>(leftMarginWidth + col * scaleX - sa->viewport()->width() / 2));
      break;
    }
    w = w->parentWidget();
  }
}

bool ContigWidget::exportPdfReport(const QString &path, int basesPerLine) {
  PrintExportOptions opt;
  opt.outputType = PrintExportOptions::PdfFile;
  opt.pdfPath = path;
  opt.basesPerLine = std::clamp(basesPerLine, 10, 200);
  opt.includeOrfA = showOrfA;
  opt.includeOrfB = showOrfB;
  QFile::remove(path);
  {
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageOrientation(QPageLayout::Portrait);
    writer.setResolution(300);
    renderAlignmentToDevice(&writer, opt);
  }
  return QFileInfo(path).size() > 0;
}

std::vector<int> ContigWidget::cutPositions(const RestEnzyme &enz, const QString &seq) const {
  std::vector<int> cuts;
  const int m = enz.motif.length(), n = seq.length();
  if (m == 0 || n < m) return cuts;
  for (int i = 0; i + m <= n; ++i) {
    bool ok = true;
    for (int j = 0; j < m && ok; ++j) if (!matchIupac(seq[i + j], enz.motif[j])) ok = false;
    if (!ok) continue;
    const int cut = i + enz.cutIdx;
    if (cut > 0 && cut < n) cuts.push_back(cut);
  }
  return cuts;
}

void ContigWidget::showVirtualGelDialog() {
  QString seq = consensusSeq.toUpper();
  seq.remove('-').remove(' ');
  if (seq.isEmpty()) {
    QMessageBox::information(this, "Virtual Gel", "There is no consensus sequence yet. Open and align reads first.");
    return;
  }
  if (restrictionEnzymes.empty()) {
    QMessageBox::information(this, "Virtual Gel", "No restriction enzymes loaded in the database.");
    return;
  }

  std::vector<GelEnzyme> list;
  {
    CursorGuard guard;
    std::map<QString, GelEnzyme> byName;
    for (const auto &enz : restrictionEnzymes) {
      std::vector<int> cuts = cutPositions(enz, seq);
      if (cuts.empty()) continue;
      GelEnzyme &ge = byName[enz.name];
      if (ge.name.isEmpty()) { ge.name = enz.name; ge.motif = enz.motif; }
      ge.cuts.insert(ge.cuts.end(), cuts.begin(), cuts.end());
      if (enz.enabled) ge.preselect = true;   // ферменты, отмеченные в "Select Enzymes to Display"
    }
    for (auto &kv : byName) {
      std::sort(kv.second.cuts.begin(), kv.second.cuts.end());
      kv.second.cuts.erase(std::unique(kv.second.cuts.begin(), kv.second.cuts.end()), kv.second.cuts.end());
      list.push_back(std::move(kv.second));
    }
  }
  if (list.empty()) {
    QMessageBox::information(this, "Virtual Gel", "None of the loaded enzymes cuts the consensus sequence.");
    return;
  }
  // Если отмечено слишком много ферментов (по умолчанию включены все), не отмечаем ни одного.
  int pre = 0;
  for (const auto &e : list) if (e.preselect) ++pre;
  if (pre > 8) for (auto &e : list) e.preselect = false;

  VirtualGelDialog dlg("Consensus", seq.length(), std::move(list), this);
  dlg.exec();
}

int ContigWidget::getColFromX(int x) const {
  int scrollX = getScrollX(); if (x < scrollX + leftMarginWidth) return -1;
  return (x - leftMarginWidth) / scaleX;
}

int ContigWidget::getTrackFromY(int y) const {
  int curY = 0;
  if (y >= 0 && y < 30) return SEL_REF;
  curY = 30;
  if (showOrfA) {
    if (y >= curY && y < curY + 14) return SEL_ORF1;
    if (y >= curY + 14 && y < curY + 27) return SEL_ORF2;
    if (y >= curY + 27 && y < curY + 40) return SEL_ORF3;
    if (y >= curY + 40 && y < curY + 53) return SEL_ORF_REV1;
    if (y >= curY + 53 && y < curY + 66) return SEL_ORF_REV2;
    if (y >= curY + 66 && y < curY + 85) return SEL_ORF_REV3;
    curY += 85;
  }
  if (y >= curY && y < curY + 30) return SEL_CONSENSUS;
  curY += 30;
  if (showOrfB) {
    if (y >= curY && y < curY + 14) return SEL_ORF_B1;
    if (y >= curY + 14 && y < curY + 27) return SEL_ORF_B2;
    if (y >= curY + 27 && y < curY + 40) return SEL_ORF_B3;
    if (y >= curY + 40 && y < curY + 53) return SEL_ORF_BREV1;
    if (y >= curY + 53 && y < curY + 66) return SEL_ORF_BREV2;
    if (y >= curY + 66 && y < curY + 85) return SEL_ORF_BREV3;
    curY += 85;
  }
  int headerY = curY + 10;
  int numIn = getNumInContig();
  int tY = headerY;
  for (size_t t = 0; t < tracks.size(); ++t) {
    if (t == (size_t)numIn && numIn < (int)tracks.size()) tY += 35;
    if (y >= tY && y < tY + trackHeight) return t;
    tY += trackHeight;
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

void ContigWidget::zoomInX() { scaleX = std::min(100.0, scaleX * 1.2); updateLayoutGeometry(); update(); }
void ContigWidget::zoomOutX() { scaleX = std::max(2.0, scaleX / 1.2); updateLayoutGeometry(); update(); }
void ContigWidget::zoomInY() { scaleY = std::min(10.0, scaleY * 1.2); updateLayoutGeometry(); update(); }
void ContigWidget::zoomOutY() { scaleY = std::max(0.001, scaleY / 1.2); updateLayoutGeometry(); update(); }
void ContigWidget::increaseTrackHeight() { trackHeight = std::min(1000, trackHeight + 15); updateLayoutGeometry(); update(); }
void ContigWidget::decreaseTrackHeight() { trackHeight = std::max(60, trackHeight - 15); updateLayoutGeometry(); update(); }
void ContigWidget::resetZoom() { scaleX = 14.0; scaleY = 0.05; trackHeight = 120; updateLayoutGeometry(); update(); }

void ContigWidget::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(event->rect(), Qt::white);
  
  int maxCols = getMaxCols();
  int scrollX = getScrollX();
  int numIn = getNumInContig();
  
  auto drawSequenceCoordinates = [&](int yTop, const QString &seq, QColor col = Qt::darkGray) {
    painter.setFont(QFont("Monospace", 6));
    painter.setPen(col);
    int rawBaseCounter = 0;
    for (int colIdx = 0; colIdx < seq.length(); ++colIdx) {
      if (seq[colIdx] != '-' && seq[colIdx] != ' ') {
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
    const QString &read = tracks[t].alignment.alignedRead.isEmpty() ? tracks[t].originalData.sequence : tracks[t].alignment.alignedRead;
    for (int i = 0; i < read.length(); ++i) {
      if (read[i] != '-' && read[i] != ' ') { if (ranges[t].first == -1) ranges[t].first = i; ranges[t].last = i; }
    }
  }
  
  int refFirst = -1, refLast = -1;
  for (int i = 0; i < referenceSeq.length(); ++i) { if (referenceSeq[i] != '-') { if (refFirst == -1) refFirst = i; refLast = i; } }
  
  int totalHeaderH = calculateHeaderHeight();
  int activeSnpCol = (currentSnpIdx >= 0 && currentSnpIdx < (int)snpColumns.size()) ? snpColumns[currentSnpIdx] : -1;
  
  for (int col = 0; col < maxCols; ++col) {
    int x = leftMarginWidth + col * scaleX;
    QChar targetBase = '?'; bool colIsMatch = true; int activeCount = 0;
    bool isSnp = std::binary_search(snpColumns.begin(), snpColumns.end(), col);   // snpColumns отсортирован по возрастанию
    if (refFirst != -1 && col >= refFirst && col <= refLast) { QChar c = referenceSeq[col].toUpper(); if (c != '-') { targetBase = c; activeCount++; } }
    for (size_t t = 0; t < tracks.size(); ++t) {
      if (!tracks[t].inContig) continue;
      if (ranges[t].first != -1 && col >= ranges[t].first && col <= ranges[t].last) {
        const QString &read = tracks[t].alignment.alignedRead.isEmpty() ? tracks[t].originalData.sequence : tracks[t].alignment.alignedRead;
        QChar c = read[col].toUpper();
        if (c == '-' || c == ' ') { colIsMatch = false; break; }
        if (targetBase == '?') { targetBase = c; activeCount++; } else if (c != targetBase) { colIsMatch = false; break; } else { activeCount++; }
      }
    }
    if (isSnp) {
      if (col == activeSnpCol) {
        painter.fillRect(x, 0, scaleX, height(), QColor(255, 87, 34, 45));
        painter.fillRect(x, 0, scaleX, totalHeaderH, QColor(255, 112, 67, 210));
        painter.setPen(QPen(QColor(211, 47, 47), 2));
        painter.drawRect(x, 0, scaleX, totalHeaderH);
      } else {
        painter.fillRect(x, 0, scaleX, totalHeaderH, QColor(255, 204, 188, 180));
      }
    } else if (colIsMatch && activeCount >= 2 && targetBase != '?') {
      painter.fillRect(x, 0, scaleX, totalHeaderH, QColor(255, 245, 157, 180));
    }
  }
  
  if (selTrack != SEL_NONE && selStartCol >= 0 && selEndCol >= 0) {
    int minC = std::min(selStartCol, selEndCol), maxC = std::max(selStartCol, selEndCol);
    int x = leftMarginWidth + minC * scaleX, w = (maxC - minC + 1) * scaleX;
    int y = 0, h = 0, baseOrfAY = 30, baseConsY = baseOrfAY + (showOrfA ? 85 : 0), baseOrfBY = baseConsY + 30;
    
    if (selTrack == SEL_REF) { y = 14; h = 18; }
    else if (selTrack == SEL_ORF1 && showOrfA) { y = baseOrfAY + 12; h = 12; }
    else if (selTrack == SEL_ORF2 && showOrfA) { y = baseOrfAY + 25; h = 12; }
    else if (selTrack == SEL_ORF3 && showOrfA) { y = baseOrfAY + 38; h = 12; }
    else if (selTrack == SEL_ORF_REV1 && showOrfA) { y = baseOrfAY + 51; h = 12; }
    else if (selTrack == SEL_ORF_REV2 && showOrfA) { y = baseOrfAY + 64; h = 12; }
    else if (selTrack == SEL_ORF_REV3 && showOrfA) { y = baseOrfAY + 77; h = 12; }
    else if (selTrack == SEL_CONSENSUS) { y = baseConsY + 14; h = 18; }
    else if (selTrack == SEL_ORF_B1 && showOrfB) { y = baseOrfBY + 12; h = 12; }
    else if (selTrack == SEL_ORF_B2 && showOrfB) { y = baseOrfBY + 25; h = 12; }
    else if (selTrack == SEL_ORF_B3 && showOrfB) { y = baseOrfBY + 38; h = 12; }
    else if (selTrack == SEL_ORF_BREV1 && showOrfB) { y = baseOrfBY + 51; h = 12; }
    else if (selTrack == SEL_ORF_BREV2 && showOrfB) { y = baseOrfBY + 64; h = 12; }
    else if (selTrack == SEL_ORF_BREV3 && showOrfB) { y = baseOrfBY + 77; h = 12; }
    else if (selTrack >= 0 && selTrack < (int)tracks.size()) { y = getTrackY(selTrack); h = trackHeight - 5; }
    
    if (h > 0) { painter.fillRect(x, y, w, h, QColor(33, 150, 243, 100)); painter.setPen(QPen(Qt::blue, 1, Qt::DashLine)); painter.drawRect(x, y, w, h); }
  }
  
  int curY = 0; drawSequenceCoordinates(curY + 12, referenceSeq, Qt::darkGreen);
  int startY = curY + 25; QFont seqFont("Monospace", 10, QFont::Bold); painter.setFont(seqFont);
  for (int i = 0; i < referenceSeq.length(); ++i) {
    int x = leftMarginWidth + i * scaleX; QChar base = referenceSeq[i].toUpper();
    if (base == 'A') painter.setPen(Qt::green); else if (base == 'C') painter.setPen(Qt::blue); else if (base == 'G') painter.setPen(Qt::black); else if (base == 'T') painter.setPen(Qt::red); else painter.setPen(Qt::gray);
    painter.drawText(x, startY, QString(base));
  }
  curY += 30;
  
  if (showOrfA) {
    int orfY[6] = { curY + 12, curY + 25, curY + 38, curY + 51, curY + 64, curY + 77 };
    QFont orfFont("Monospace", 8, QFont::Bold); painter.setFont(orfFont);
    for (int frame = 0; frame < 6; ++frame) {
      int yPos = orfY[frame]; QString orfSeq = getOrfLine(frame, referenceSeq);
      for (int col = 0; col < orfSeq.length(); ++col) {
        QChar aa = orfSeq[col];
        if (aa != ' ') {
          int x = leftMarginWidth + col * scaleX;
          if (aa == 'M') painter.setPen(Qt::darkGreen); else if (aa == '*') painter.setPen(Qt::red); else if (frame >= 3) painter.setPen(QColor(230, 81, 0)); else painter.setPen(Qt::darkBlue);
          painter.drawText(x, yPos, QString(aa));
        }
      }
    }
    curY += 85;
  }
  
  drawSequenceCoordinates(curY + 12, consensusSeq, QColor(156, 39, 176));
  
  if (showRestrictionSites && !consensusSeq.isEmpty() && !restrictionEnzymes.empty()) {
    QString ungapped; std::vector<int> colMap;
    for (int i = 0; i < consensusSeq.length(); ++i) {
      if (consensusSeq[i] != '-') {
        ungapped += consensusSeq[i].toUpper();
        colMap.push_back(i);
      }
    }
    int consY = curY + 10;
    std::map<int, QStringList> cutsByCol;
    
    for (const auto& enz : restrictionEnzymes) {
      if (!enz.enabled) continue;
      int mLen = enz.motif.length();
      if (ungapped.length() < mLen) continue;
      for (int pos = 0; pos <= ungapped.length() - mLen; ++pos) {
        bool match = true;
        for (int j = 0; j < mLen; ++j) {
          if (!matchIupac(ungapped[pos + j], enz.motif[j])) { match = false; break; }
        }
        if (match) {
          int cutPoint = pos + enz.cutIdx;
          if (cutPoint >= 0 && cutPoint < (int)colMap.size()) {
            int realCol = colMap[cutPoint];
            if (!cutsByCol[realCol].contains(enz.name)) {
              cutsByCol[realCol].append(enz.name);
            }
          }
        }
      }
    }
    
    painter.setFont(QFont("SansSerif", 7, QFont::Bold));
    for (const auto& [realCol, names] : cutsByCol) {
      int x = leftMarginWidth + realCol * scaleX;
      painter.setPen(Qt::darkRed);
      painter.drawLine(x, consY - 8, x, consY + 3);
      
      QString label = names.join("/");
      painter.save();
      painter.translate(x + 2, consY - 10);
      painter.rotate(-45);
      painter.drawText(0, 0, label);
      painter.restore();
    }
  }
  
  startY = curY + 25; painter.setFont(seqFont);
  for (int i = 0; i < consensusSeq.length(); ++i) {
    int x = leftMarginWidth + i * scaleX; QChar base = consensusSeq[i].toUpper();
    if (base == 'A') painter.setPen(Qt::green); else if (base == 'C') painter.setPen(Qt::blue); else if (base == 'G') painter.setPen(Qt::black); else if (base == 'T') painter.setPen(Qt::red); else if (base == '-') painter.setPen(Qt::gray); else painter.setPen(QColor(156, 39, 176));
    painter.drawText(x, startY, QString(base));
  }
  curY += 30;
  
  if (showOrfB) {
    int orfY[6] = { curY + 12, curY + 25, curY + 38, curY + 51, curY + 64, curY + 77 };
    QFont orfFont("Monospace", 8, QFont::Bold); painter.setFont(orfFont);
    for (int frame = 0; frame < 6; ++frame) {
      int yPos = orfY[frame]; QString orfSeq = getOrfLine(frame, consensusSeq);
      for (int col = 0; col < orfSeq.length(); ++col) {
        QChar aa = orfSeq[col];
        if (aa != ' ') {
          int x = leftMarginWidth + col * scaleX;
          if (aa == 'M') painter.setPen(Qt::darkGreen); else if (aa == '*') painter.setPen(Qt::red); else if (frame >= 3) painter.setPen(QColor(230, 81, 0)); else painter.setPen(Qt::darkBlue);
          painter.drawText(x, yPos, QString(aa));
        }
      }
    }
    curY += 85;
  }
  
  int currentY = curY + 10;
  for (size_t t = 0; t < tracks.size(); ++t) {
    if (t == (size_t)numIn && numIn < (int)tracks.size()) {
      int sepY = currentY + 15;
      painter.setPen(QPen(Qt::red, 2, Qt::SolidLine)); painter.drawLine(0, sepY, width(), sepY);
      painter.fillRect(scrollX + leftMarginWidth + 10, sepY - 12, 480, 24, Qt::white);
      painter.setPen(Qt::red); QFont sepFont("SansSerif", 9, QFont::Bold); painter.setFont(sepFont);
      painter.drawText(scrollX + leftMarginWidth + 15, sepY + 4, QString("⛔ Sequences below not included in contig (Identity < %1%)").arg(minIdentityThreshold, 0, 'f', 1));
      currentY += 35;
    }
    
    auto &track = tracks[t];
    Ab1Data dispData = Ab1Parser::getOrientedData(track.originalData, track.alignment.isReverseComplement);
    const QString &readAligned = track.alignment.alignedRead.isEmpty() ? dispData.sequence : track.alignment.alignedRead;
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
      int x = leftMarginWidth + col * scaleX; QChar base = readAligned[col];
      QChar refC = (col < referenceSeq.length()) ? referenceSeq[col].toUpper() : '?';
      bool isMismatchLetter = (base.isLetter() && base != '-' && base != ' ' && refC.isLetter() && refC != '?' && refC != '-' && base.toUpper() != refC);
      
      if (isMismatchLetter) { painter.fillRect(x - 1, currentY + 7, scaleX, 15, QColor(229, 57, 53)); painter.setPen(Qt::white); } 
      else {
        if (base == 'A') painter.setPen(Qt::green); else if (base == 'C') painter.setPen(Qt::blue); else if (base == 'G') painter.setPen(Qt::black); else if (base == 'T') painter.setPen(Qt::red); else painter.setPen(Qt::gray);
      }
      if (base != '-' && base != ' ') {
        painter.drawText(x, currentY + 20, QString(base));
      }
      
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
        if (trace.empty()) return; QPainterPath path;
        for (size_t bp = 0; bp + 1 < numBases; ++bp) {
          int startS = ploc[bp], endS = ploc[bp + 1];
          if (startS >= (int)trace.size()) continue;
          endS = std::min(endS, (int)trace.size() - 1);
          if (startS >= endS) continue;
          bool hasGap = (basePoints[bp + 1].col > basePoints[bp].col + 1);
          int midS = (startS + endS) / 2;
          for (int s = startS; s <= endS; ++s) {
            double xPos;
            if (!hasGap) { double t_param = double(s - startS) / std::max(1, endS - startS); xPos = basePoints[bp].x + t_param * (basePoints[bp + 1].x - basePoints[bp].x); } 
            else {
              if (s <= midS) { double t_param = double(s - startS) / std::max(1, midS - startS); xPos = basePoints[bp].x + t_param * (scaleX * 0.5); } 
              else { double t_param = double(s - midS) / std::max(1, endS - midS); xPos = (basePoints[bp + 1].x - scaleX * 0.5) + t_param * (scaleX * 0.5); }
            }
            double yPos = baseline - trace[s] * scaleY;
            if (bp == 0 && s == startS) { path.moveTo(xPos, yPos); } else if (hasGap && s == midS + 1) { path.moveTo(xPos, baseline); path.lineTo(xPos, yPos); } else { path.lineTo(xPos, yPos); }
          }
        }
        painter.setPen(QPen(col, 1.4)); painter.drawPath(path);
      };
      drawContinuousTrace(dispData.traceA, Qt::green); drawContinuousTrace(dispData.traceC, Qt::blue); drawContinuousTrace(dispData.traceG, Qt::black); drawContinuousTrace(dispData.traceT, Qt::red);
    }
    painter.setPen(Qt::lightGray); painter.drawLine(0, currentY + trackHeight - 5, width(), currentY + trackHeight - 5); currentY += trackHeight;
  }
  
  // Вертикальная направляющая линия (crosshair) при наведении курсора
  if (hoverCol >= 0 && hoverCol < maxCols) {
    int hx = leftMarginWidth + hoverCol * scaleX;
    painter.fillRect(hx, 0, scaleX, height(), QColor(66, 165, 245, 40));
    painter.setPen(QPen(QColor(25, 118, 210, 180), 1, Qt::DashLine));
    painter.drawLine(hx + scaleX / 2.0, 0, hx + scaleX / 2.0, height());
  }
  
  // Фиксированная левая панель
  painter.fillRect(scrollX, 0, leftMarginWidth - 5, height(), Qt::white);
  painter.setPen(QPen(Qt::gray, 2)); painter.drawLine(scrollX + leftMarginWidth - 5, 0, scrollX + leftMarginWidth - 5, height());
  
  curY = 0; painter.setFont(seqFont); painter.setPen(Qt::darkGreen); painter.drawText(scrollX + 10, curY + 25, isAutoReference ? "CONTIG (Auto):" : "REF (FASTA):"); curY += 30;
  
  if (showOrfA) {
    painter.setFont(QFont("SansSerif", 7)); painter.setPen(Qt::darkGray);
    painter.drawText(scrollX + 10, curY + 12, "ORF A +1:"); painter.drawText(scrollX + 10, curY + 25, "ORF A +2:"); painter.drawText(scrollX + 10, curY + 38, "ORF A +3:");
    painter.setPen(QColor(230, 81, 0));
    painter.drawText(scrollX + 10, curY + 51, "ORF A -1:"); painter.drawText(scrollX + 10, curY + 64, "ORF A -2:"); painter.drawText(scrollX + 10, curY + 77, "ORF A -3:");
    curY += 85;
  }
  
  painter.setFont(seqFont); painter.setPen(QColor(156, 39, 176)); painter.drawText(scrollX + 10, curY + 25, "CONSENSUS:"); curY += 30;
  
  if (showOrfB) {
    painter.setFont(QFont("SansSerif", 7)); painter.setPen(Qt::darkGray);
    painter.drawText(scrollX + 10, curY + 12, "ORF B +1:"); painter.drawText(scrollX + 10, curY + 25, "ORF B +2:"); painter.drawText(scrollX + 10, curY + 38, "ORF B +3:");
    painter.setPen(QColor(230, 81, 0));
    painter.drawText(scrollX + 10, curY + 51, "ORF B -1:"); painter.drawText(scrollX + 10, curY + 64, "ORF B -2:"); painter.drawText(scrollX + 10, curY + 77, "ORF B -3:");
    curY += 85;
  }
  
  QFont labelFont("SansSerif", 9);
  for (size_t t = 0; t < tracks.size(); ++t) {
    int tY = getTrackY(t); QRect nameRect(scrollX + 10, tY, leftMarginWidth - 25, trackHeight - 5);
    painter.setFont(labelFont); painter.setPen(tracks[t].inContig ? Qt::darkBlue : Qt::red);
    QString rcMark = tracks[t].alignment.isReverseComplement ? " [RC]" : "";
    QString labelText = QString("%1%2 (%3%)").arg(tracks[t].fileName).arg(rcMark).arg(tracks[t].alignment.identity, 0, 'f', 1);
    painter.drawText(nameRect, Qt::TextWordWrap | Qt::TextWrapAnywhere | Qt::AlignLeft | Qt::AlignTop, labelText);
  }
}

void ContigWidget::leaveEvent(QEvent *event) { QWidget::leaveEvent(event); if (hoverCol != -1) { hoverCol = -1; update(); } }
void ContigWidget::dragEnterEvent(QDragEnterEvent *event) { if (event->mimeData()->hasUrls()) event->acceptProposedAction(); }
void ContigWidget::dropEvent(QDropEvent *event) {
  for (const QUrl &url : event->mimeData()->urls()) {
    QString filePath = url.toLocalFile(); if (filePath.isEmpty()) continue;
    if (filePath.endsWith(".ChromaZ", Qt::CaseInsensitive)) {
      if (modifiedFlag && !tracks.empty() &&
          QMessageBox::question(this, "Open Project",
              "Opening a project replaces the current workspace, which has unsaved changes.\nContinue?",
              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
      if (!loadProject(filePath)) {
        QMessageBox::warning(this, "Open Project",
                             "The project could not be loaded. The file may be unreadable, corrupted, or not a ChromaZ project.");
      }
      return;
    }
    if (filePath.endsWith(".fastq", Qt::CaseInsensitive) || filePath.endsWith(".fq", Qt::CaseInsensitive)) {
      auto records = SequenceFileParser::parseFastq(filePath);
      for (const auto &rec : records) { QString trackId = (records.size() > 1) ? filePath + " [" + rec.name + "]" : filePath; addAb1Track(trackId, rec.data); }
      continue;
    }
    if (filePath.endsWith(".txt", Qt::CaseInsensitive)) {
      auto records = SequenceFileParser::parseTxt(filePath);
      for (const auto &rec : records) { QString trackId = (records.size() > 1) ? filePath + " [" + rec.name + "]" : filePath; addAb1Track(trackId, rec.data); }
      continue;
    }
    if (isFileLoaded(filePath)) continue;
    if (filePath.endsWith(".ab1", Qt::CaseInsensitive) || filePath.endsWith(".scf", Qt::CaseInsensitive)) {
      Ab1Data data = Ab1Parser::parse(filePath); if (data.isValid) addAb1Track(filePath, data);
    } else if (filePath.endsWith(".fasta", Qt::CaseInsensitive) || filePath.endsWith(".fa", Qt::CaseInsensitive) || filePath.endsWith(".gb", Qt::CaseInsensitive) || filePath.endsWith(".gbk", Qt::CaseInsensitive)) {
      auto records = SequenceFileParser::parseFastaOrGbRecords(filePath);
      for (const auto &rec : records) { QString trackId = (records.size() > 1) ? filePath + " [" + rec.name + "]" : filePath; addAb1Track(trackId, rec.data); }
    }
  }
  runAlignment();
}

void ContigWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    int x = event->position().x(), scrollX = getScrollX();
    if (std::abs(x - (scrollX + leftMarginWidth - 5)) <= 8) { isResizingMargin = true; return; }
    int c = getColFromX(x), t = getTrackFromY(event->position().y());
    if (x < scrollX + leftMarginWidth && t >= 0) { if (!isAligning()) dragSourceTrackIdx = t; return; }
    if (c >= 0 && t != SEL_NONE) { selTrack = t; selStartCol = c; selEndCol = c; isSelecting = true; update(); }
  }
}

void ContigWidget::mouseMoveEvent(QMouseEvent *event) {
  int x = event->position().x(), scrollX = getScrollX(), y = event->position().y(), oldHover = hoverCol;
  if (!isSelecting && !isResizingMargin && dragSourceTrackIdx < 0) hoverCol = getColFromX(x); else hoverCol = -1;
  if (dragSourceTrackIdx >= 0 && (event->buttons() & Qt::LeftButton)) {
    int targetTrack = getTrackFromY(y);
    if (targetTrack >= 0 && targetTrack < (int)tracks.size() && targetTrack != dragSourceTrackIdx) {
      if (tracks[dragSourceTrackIdx].inContig == tracks[targetTrack].inContig) { pushUndoState(); std::swap(tracks[dragSourceTrackIdx], tracks[targetTrack]); dragSourceTrackIdx = targetTrack; update(); }
    }
    return;
  }
  if (isResizingMargin) { leftMarginWidth = std::clamp(x - scrollX, 80, 800); updateLayoutGeometry(); update(); return; }
  if (std::abs(x - (scrollX + leftMarginWidth - 5)) <= 8) setCursor(Qt::SplitHCursor); else if (!isSelecting) unsetCursor();
  if (isSelecting) { int c = getColFromX(x); if (c >= 0) { selEndCol = c; update(); } }
  if (hoverCol != oldHover) update();
}

void ContigWidget::mouseReleaseEvent(QMouseEvent *event) { if (event->button() == Qt::LeftButton) { dragSourceTrackIdx = -2; if (isResizingMargin) { isResizingMargin = false; unsetCursor(); } isSelecting = false; } }

void ContigWidget::keyPressEvent(QKeyEvent *event) {
  if (event->matches(QKeySequence::Undo)) { undo(); return; }
  if (event->matches(QKeySequence::Redo)) { redo(); return; }
  if (event->matches(QKeySequence::Copy)) { copySelectedToClipboard(); return; }
  if (isAligning()) return;   // правки во время выравнивания не принимаются
  if (selTrack == SEL_NONE || selStartCol < 0) return;
  int col = selStartCol; QString text = event->text().toUpper();
  
  if (selTrack == SEL_REF) {
    if (!text.isEmpty() && (text == "A" || text == "C" || text == "G" || text == "T" || text == "N" || text == "-")) {
      pushUndoState(); if (col < referenceSeq.length()) referenceSeq[col] = text[0]; else referenceSeq.append(text[0]); calculateConsensusAndSNPs(); updateLayoutGeometry(); update();
    } else if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
      if (col < referenceSeq.length()) { pushUndoState(); if (referenceSeq[col] != '-') referenceSeq[col] = '-'; else referenceSeq.remove(col, 1); calculateConsensusAndSNPs(); updateLayoutGeometry(); update(); }
    } else if (event->key() == Qt::Key_Left) { selStartCol = std::max(0, selStartCol - 1); selEndCol = selStartCol; update(); } 
    else if (event->key() == Qt::Key_Right) { selStartCol++; selEndCol = selStartCol; update(); }
    return;
  }
  
  if (selTrack == SEL_CONSENSUS) {
    if (!text.isEmpty() && (text == "A" || text == "C" || text == "G" || text == "T" || text == "N" || text == "-")) {
      pushUndoState(); if (col < consensusSeq.length()) consensusSeq[col] = text[0]; else consensusSeq.append(text[0]); update();
    } else if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
      if (col < consensusSeq.length()) { pushUndoState(); if (consensusSeq[col] != '-') consensusSeq[col] = '-'; else consensusSeq.remove(col, 1); update(); }
    } else if (event->key() == Qt::Key_Left) { selStartCol = std::max(0, selStartCol - 1); selEndCol = selStartCol; update(); } 
    else if (event->key() == Qt::Key_Right) { selStartCol++; selEndCol = selStartCol; update(); }
    return;
  }
  
  if (selTrack >= 0 && selTrack < (int)tracks.size()) {
    auto &track = tracks[selTrack]; auto &read = track.alignment.alignedRead; auto &origSeq = track.originalData.sequence;
    auto &qScores = track.originalData.qualityScores; auto &bPos = track.originalData.basePositions; bool isRC = track.alignment.isReverseComplement;
    if (event->key() == Qt::Key_Left) { selStartCol = std::max(0, selStartCol - 1); selEndCol = selStartCol; update(); return; } 
    else if (event->key() == Qt::Key_Right) { selStartCol++; selEndCol = selStartCol; update(); return; }
    if (col >= read.length()) return;
    int rawIdx = 0; for (int i = 0; i < col && i < read.length(); ++i) { if (read[i] != '-') rawIdx++; }
    
    if (!text.isEmpty() && (text == "A" || text == "C" || text == "G" || text == "T" || text == "N" || text == "-")) {
      pushUndoState(); QChar newChar = text[0], oldChar = read[col]; read[col] = newChar;
      QChar origNewChar = isRC ? Ab1Parser::reverseComplement(QString(newChar))[0] : newChar;
      if (oldChar != '-' && newChar != '-') {
        int origIdx = isRC ? ((int)origSeq.length() - 1 - rawIdx) : rawIdx;
        if (origIdx >= 0 && origIdx < origSeq.length()) origSeq[origIdx] = origNewChar;
      } 
      else if (oldChar == '-' && newChar != '-') {
        int origIdx = isRC ? ((int)origSeq.length() - rawIdx) : rawIdx;
        if (origIdx >= 0 && origIdx <= origSeq.length()) origSeq.insert(origIdx, origNewChar);
        // У ридов без хроматограммы (FASTA/TXT) массивов качества и пиков нет — не создаём их из одного элемента.
        if (!qScores.empty()) {
          int qIdx = isRC ? ((int)qScores.size() - rawIdx) : rawIdx;
          if (qIdx >= 0 && qIdx <= (int)qScores.size()) qScores.insert(qScores.begin() + qIdx, 30);
        }
        if (!bPos.empty()) {
          int pIdx = isRC ? ((int)bPos.size() - rawIdx) : rawIdx;
          if (pIdx >= 0 && pIdx <= (int)bPos.size()) {
            // Пик новой базы — строго между соседними пиками, чтобы не нарушать монотонность трассы.
            int posVal;
            if (pIdx > 0 && pIdx < (int)bPos.size()) posVal = (bPos[pIdx - 1] + bPos[pIdx]) / 2;
            else if (pIdx > 0) posVal = bPos[pIdx - 1] + 10;
            else posVal = std::max(0, bPos[0] - 10);
            bPos.insert(bPos.begin() + pIdx, posVal);
          }
        }
      } else if (oldChar != '-' && newChar == '-') {
        read[col] = ' ';
        int origIdx = isRC ? ((int)origSeq.length() - 1 - rawIdx) : rawIdx;
        if (origIdx >= 0 && origIdx < origSeq.length()) origSeq[origIdx] = ' ';
      }
      calculateConsensusAndSNPs(); updateLayoutGeometry(); update();
    } else if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
      QChar oldChar = read[col];
      if (oldChar == ' ') return;   // основание уже удалено; слот хранит данные трассы — колонку не убираем
      pushUndoState();
      if (oldChar != '-') {
        read[col] = ' ';
        int origIdx = isRC ? ((int)origSeq.length() - 1 - rawIdx) : rawIdx;
        if (origIdx >= 0 && origIdx < origSeq.length()) origSeq[origIdx] = ' ';
      } else {
        read.remove(col, 1);        // обычный гэп: схлопываем колонку
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
  else if (selTrack == SEL_ORF1) seq = getOrfLine(0, referenceSeq).mid(minC, len);
  else if (selTrack == SEL_ORF2) seq = getOrfLine(1, referenceSeq).mid(minC, len);
  else if (selTrack == SEL_ORF3) seq = getOrfLine(2, referenceSeq).mid(minC, len);
  else if (selTrack == SEL_ORF_REV1) seq = getOrfLine(3, referenceSeq).mid(minC, len);
  else if (selTrack == SEL_ORF_REV2) seq = getOrfLine(4, referenceSeq).mid(minC, len);
  else if (selTrack == SEL_ORF_REV3) seq = getOrfLine(5, referenceSeq).mid(minC, len);
  else if (selTrack == SEL_CONSENSUS) seq = consensusSeq.mid(minC, len);
  else if (selTrack == SEL_ORF_B1) seq = getOrfLine(0, consensusSeq).mid(minC, len);
  else if (selTrack == SEL_ORF_B2) seq = getOrfLine(1, consensusSeq).mid(minC, len);
  else if (selTrack == SEL_ORF_B3) seq = getOrfLine(2, consensusSeq).mid(minC, len);
  else if (selTrack == SEL_ORF_BREV1) seq = getOrfLine(3, consensusSeq).mid(minC, len);
  else if (selTrack == SEL_ORF_BREV2) seq = getOrfLine(4, consensusSeq).mid(minC, len);
  else if (selTrack == SEL_ORF_BREV3) seq = getOrfLine(5, consensusSeq).mid(minC, len);
  else if (selTrack >= 0 && selTrack < (int)tracks.size()) {
    QString full = tracks[selTrack].alignment.alignedRead.isEmpty() ?
    tracks[selTrack].originalData.sequence :
    tracks[selTrack].alignment.alignedRead;
    seq = full.mid(minC, len);
  }
  if (!seq.isEmpty()) QGuiApplication::clipboard()->setText(seq);
}

void ContigWidget::copySelectionAsImageToClipboard() {
  if (selStartCol < 0 || selEndCol < 0) return;
  
  int minC = std::min(selStartCol, selEndCol);
  int maxC = std::max(selStartCol, selEndCol);
  int numCols = maxC - minC + 1;
  
  int imgWidth = numCols * scaleX + leftMarginWidth;
  int imgHeight = getTrackY(tracks.size()) + 20;
  
  QImage image(imgWidth, imgHeight, QImage::Format_ARGB32);
  if (image.isNull()) return;
  image.fill(Qt::white);
  
  QPainter imgPainter(&image);
  imgPainter.setRenderHint(QPainter::Antialiasing);
  imgPainter.translate(-minC * scaleX, 0);
  
  render(&imgPainter);
  QGuiApplication::clipboard()->setImage(image);
}

void ContigWidget::wheelEvent(QWheelEvent *event) {
  if (event->modifiers() & Qt::ControlModifier) {
    if (event->angleDelta().y() > 0) zoomInY(); else zoomOutY();
  } else {
    if (event->angleDelta().y() > 0) zoomInX(); else zoomOutX();
  }
}

void ContigWidget::contextMenuEvent(QContextMenuEvent *event) {
  if (isAligning()) return;
  QMenu menu(this);
  QAction *copyAct = menu.addAction("📋 Copy Sequence (Ctrl+C)");
  connect(copyAct, &QAction::triggered, this, &ContigWidget::copySelectedToClipboard);
  QAction *copyImgAct = menu.addAction("🖼 Copy Selection as Image");
  connect(copyImgAct, &QAction::triggered, this, &ContigWidget::copySelectionAsImageToClipboard);
  
  menu.addSeparator();
  
  if (selTrack >= SEL_CONSENSUS || selTrack == SEL_REF || selTrack >= 0) {
    QAction *blastAct = menu.addAction("🔍 Send to NCBI BLASTn");
    connect(blastAct, &QAction::triggered, this, &ContigWidget::openInNcbiBlast);
    QAction *propAct = menu.addAction("📊 Sequence & Protein Properties");
    connect(propAct, &QAction::triggered, this, &ContigWidget::showSequenceProperties);
    menu.addSeparator();
  }
  
  QAction *readAct = menu.addAction("🔊 Read Sequence Aloud");
  connect(readAct, &QAction::triggered, this, &ContigWidget::readSelectionAloud);
  QAction *stopReadAct = menu.addAction("🔇 Stop Reading");
  connect(stopReadAct, &QAction::triggered, this, &ContigWidget::stopReading);
  
  QMenu *voiceMenu = menu.addMenu("🎙 Voice");
  if (tts) {
    const auto voices = tts->availableVoices();
    for (const QVoice &v : voices) {
      QString label = QString("%1 (%2)").arg(v.name()).arg(QLocale::languageToString(v.locale().language()));
      QAction *vAct = voiceMenu->addAction(label);
      vAct->setCheckable(true);
      if (v.name() == tts->voice().name()) vAct->setChecked(true);
      connect(vAct, &QAction::triggered, this, [this, v]() { setVoice(v); });
    }
  }
  
  QMenu *speedMenu = menu.addMenu("⏱ Speech Speed");
  struct SpeedPreset { QString label; double rate; };
  const std::vector<SpeedPreset> speeds = {
    {"0.5x (Very Slow)", -1.0},
    {"0.75x (Slow)", -0.5},
    {"1.0x (Normal)", 0.0},
    {"1.25x (Fast)", 0.25},
    {"1.5x (Faster)", 0.5},
    {"1.75x (Very Fast)", 0.75},
    {"2.0x (Double Speed)", 1.0}
  };
  for (const auto &sp : speeds) {
    QAction *spAct = speedMenu->addAction(sp.label);
    spAct->setCheckable(true);
    if (std::abs(speechRate - sp.rate) < 0.1) spAct->setChecked(true);
    connect(spAct, &QAction::triggered, this, [this, r = sp.rate]() { setSpeechRate(r); });
  }
  
  menu.addSeparator();
  QAction *printAct = menu.addAction("🖨 Print & Export PDF Report...");
  connect(printAct, &QAction::triggered, this, &ContigWidget::showPrintExportDialog);
  
  QAction *genBankAct = menu.addAction("📥 Export Annotated GenBank (.gbk)...");
  connect(genBankAct, &QAction::triggered, this, &ContigWidget::exportConsensusGenBankPrompt);
  
  QAction *resSitesAct = menu.addAction(showRestrictionSites ? "👁 Hide Restriction Sites" : "👁 Show Restriction Sites");
  connect(resSitesAct, &QAction::triggered, this, [this]() { toggleRestrictionSites(!showRestrictionSites); });
  
  QAction *chooseResAct = menu.addAction("⚙ Select Enzymes to Display...");
  connect(chooseResAct, &QAction::triggered, this, &ContigWidget::showEnzymeSelectionDialog);
  QAction *gelAct = menu.addAction("🧫 Virtual Gel...");
  connect(gelAct, &QAction::triggered, this, &ContigWidget::showVirtualGelDialog);
  
  QAction *trimVecAct = menu.addAction("✂️ Trim Vectors (UniVec)");
  connect(trimVecAct, &QAction::triggered, this, &ContigWidget::trimVectors);
  
  int t = getTrackFromY(event->pos().y());
  if (t >= 0 && t < (int)tracks.size()) {
    menu.addSeparator();
    QString rcTitle = tracks[t].alignment.isReverseComplement ? "🔄 Revert to Forward Strand" : "🔄 Convert to Reverse Complement";
    QAction *toggleRCAct = menu.addAction(rcTitle);
    connect(toggleRCAct, &QAction::triggered, this, [this, t]() {
      pushUndoState(); tracks[t].alignment.isReverseComplement = !tracks[t].alignment.isReverseComplement; tracks[t].overrideRC = true; suppressUndo = true; runAlignment(); suppressUndo = false;
    });
    QAction *deleteTrackAct = menu.addAction("❌ Delete Track");
    connect(deleteTrackAct, &QAction::triggered, this, [this, t]() {
      if (t >= 0 && t < (int)tracks.size()) { pushUndoState(); tracks.erase(tracks.begin() + t); calculateConsensusAndSNPs(); fitToWindowHeight(); updateLayoutGeometry(); update(); }
    });
  }
  menu.exec(event->globalPos());
}
