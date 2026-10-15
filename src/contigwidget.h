#ifndef CONTIGWIDGET_H
#define CONTIGWIDGET_H

#include <QWidget>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <vector>
#include <QtTextToSpeech/QTextToSpeech>
#include <QtTextToSpeech/QVoice>
#include <QPagedPaintDevice>
#include "ab1parser.h"
#include "alignmentengine.h"

static const int SEL_NONE = -100;
static const int SEL_REF = -1;
static const int SEL_CONSENSUS = -2;
static const int SEL_ORF1 = -3;
static const int SEL_ORF2 = -4;
static const int SEL_ORF3 = -5;
static const int SEL_ORF_REV1 = -6;
static const int SEL_ORF_REV2 = -7;
static const int SEL_ORF_REV3 = -8;
static const int SEL_ORF_B1 = -9;
static const int SEL_ORF_B2 = -10;
static const int SEL_ORF_B3 = -11;
static const int SEL_ORF_BREV1 = -12;
static const int SEL_ORF_BREV2 = -13;
static const int SEL_ORF_BREV3 = -14;

struct AlignedTrack {
  QString filePath;
  QString fileName;
  Ab1Data originalData;
  AlignmentResult alignment;
  bool inContig = true;
  bool overrideRC = false;
};

struct SearchMatch {
  int trackId;
  int startCol;
  int length;
};

struct WidgetState {
  QString referenceSeq;
  QString consensusSeq;
  std::vector<AlignedTrack> tracks;
  int selTrack;
  int selStartCol;
  int selEndCol;
  bool autoRCEnabled;
  bool showOrfA;
  bool showOrfB;
};

struct RestEnzyme {
  QString name;
  QString motif;
  int cutIdx;
};

struct PrintExportOptions {
  enum OutputType { PdfFile, SystemPrinter };
  OutputType outputType = PdfFile;
  QString pdfPath = "ChromaZ_Alignment.pdf";
  int basesPerLine = 60;
  bool includeTraces = true;
  bool includeQuality = true;
  bool includeOrfA = true;
  bool includeOrfB = true;
  bool includeRef = true;
  bool includeConsensus = true;
};

class ContigWidget : public QWidget {
  Q_OBJECT
  
public:
  explicit ContigWidget(QWidget *parent = nullptr);
  
  void setReference(const QString &seq);
  bool addAb1Track(const QString &filePath, const Ab1Data &data);
  bool isFileLoaded(const QString &filePath) const;
  void fitToWindowHeight(int viewportHeight = -1);
  void clearAll();
  
  QString getConsensusSequence() const { return consensusSeq; }
  bool isIupacConsensusEnabled() const { return useIupacConsensus; }
  bool isAutoRCEnabled() const { return autoRCEnabled; }
  bool isOrfAEnabled() const { return showOrfA; }
  bool isOrfBEnabled() const { return showOrfB; }
  bool isRestrictionSitesVisible() const { return showRestrictionSites; }
  AlignmentAlgorithm getAlignmentAlgorithm() const { return currentAlgorithm; }
  
  bool saveProject(const QString &filePath);
  bool loadProject(const QString &filePath);
  
  QList<QVoice> getAvailableVoices() const;
  QVoice getCurrentVoice() const;
  double getSpeechRate() const { return speechRate; }
  
  signals:
    void projectLoaded(const QString &filePath);
  void autoRCChanged(bool enabled);
  void restrictionSitesToggled(bool visible);
  void speechRateChanged(double rate);
  void voiceChanged(const QVoice &voice);
  
  public slots:
    void undo();
  void redo();
  void runAlignment();
  void setAutoRC(bool enable);
  void setShowOrfA(bool enable);
  void setShowOrfB(bool enable);
  void setAlignmentAlgorithm(int algoIdx);
  void setTranslationTable(int tableIdx);
  void setConsensusIupac(bool enable);
  void setMinIdentityThreshold(double val);
  
  void trimLowQualityEnds(int minPhred = 20);
  void trimVectors();
  void toggleRestrictionSites(bool enable);
  
  void readSelectionAloud();
  void stopReading();
  void setSpeechRate(double rate);
  void setVoice(const QVoice &voice);
  
  void openInNcbiBlast();
  void showMeltingTemperature();
  void showSequenceProperties();
  void exportConsensusGenBank(const QString &filePath);
  void exportConsensusGenBankPrompt();
  
  // Единое модальное окно печати и экспорта отчетов
  void showPrintExportDialog();
  void renderAlignmentToDevice(QPagedPaintDevice *device, const PrintExportOptions &opt);
  
  void jumpNextSNP();
  void jumpPrevSNP();
  void findSequence(const QString &query, bool forward = true);
  void exportConsensusFasta(const QString &filePath);
  void exportViewImage(const QString &filePath);
  void zoomInX(); void zoomOutX();
  void zoomInY(); void zoomOutY();
  void increaseTrackHeight(); void decreaseTrackHeight();
  void resetZoom();
  void copySelectedToClipboard();
  void copySelectionAsImageToClipboard();
  
protected:
  void paintEvent(QPaintEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  void leaveEvent(QEvent *event) override;
  
private:
  void updateLayoutGeometry();
  void calculateConsensusAndSNPs();
  int getScrollX() const;
  int getColFromX(int x) const;
  int getTrackFromY(int y) const;
  int getMaxCols() const;
  int getNumInContig() const;
  int getTrackY(int trackIdx) const;
  int calculateHeaderHeight() const;
  QChar translateCodon(const QString& codon, int tableIndex) const;
  QString getOrfLine(int frame, const QString &targetSeq) const;
  void pushUndoState();
  void applyTrimToTrack(AlignedTrack &tr, int trimLeftCount, int trimRightCount);
  int findFuzzyMatch(const QString& text, const QString& pattern, int maxMismatches);
  void loadRestrictionEnzymes();
  bool matchIupac(QChar seqChar, QChar motifChar) const;
  QString getSelectedSequence() const;
  
  QString referenceSeq;
  QString consensusSeq;
  std::vector<int> snpColumns;
  int currentSnpIdx = -1;
  
  std::vector<SearchMatch> searchMatches;
  int currentMatchIdx = -1;
  QString lastSearchQuery;
  
  bool isAutoReference = false;
  std::vector<AlignedTrack> tracks;
  
  std::vector<WidgetState> undoStack;
  std::vector<WidgetState> redoStack;
  
  AlignmentAlgorithm currentAlgorithm = AlignmentAlgorithm::OverlapCAP3;
  bool useIupacConsensus = false;
  bool autoRCEnabled = true;
  bool showOrfA = true;
  bool showOrfB = true;
  int translationTable = 0;
  double minIdentityThreshold = 45.0;
  int leftMarginWidth = 220;
  bool isResizingMargin = false;
  int dragSourceTrackIdx = -2;
  
  int hoverCol = -1;
  
  double scaleX = 14.0;
  double scaleY = 0.05;
  int trackHeight = 120;
  
  int selTrack = SEL_NONE;      
  int selStartCol = -1;   
  int selEndCol = -1;     
  bool isSelecting = false;
  
  QTextToSpeech *tts = nullptr;
  double speechRate = 0.0;
  bool showRestrictionSites = false;
  std::vector<RestEnzyme> restrictionEnzymes;
};

#endif // CONTIGWIDGET_H