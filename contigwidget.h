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
  AlignmentAlgorithm getAlignmentAlgorithm() const { return currentAlgorithm; }
  
  bool saveProject(const QString &filePath);
  bool loadProject(const QString &filePath);
  
  signals:
    void projectLoaded(const QString &filePath);
  void autoRCChanged(bool enabled);
  
  public slots:
    void undo();
  void redo();
  void runAlignment();
  void setAutoRC(bool enable);
  void setAlignmentAlgorithm(int algoIdx);
  void setTranslationTable(int tableIdx);
  void setConsensusIupac(bool enable);
  void setMinIdentityThreshold(double val);
  void trimLowQualityEnds(int minPhred = 20);
  void jumpNextSNP();
  void jumpPrevSNP();
  void findSequence(const QString &query, bool forward = true);
  void exportConsensusFasta(const QString &filePath);
  void exportViewImage(const QString &filePath);
  void exportPdf(const QString &filePath, int basesPerLine = 80);
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
  QChar translateCodon(const QString& codon, int tableIndex) const;
  QString getOrfLine(int frame) const;
  void pushUndoState();
  
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
  int translationTable = 0;
  double minIdentityThreshold = 45.0;
  int leftMarginWidth = 220;
  bool isResizingMargin = false;
  int dragSourceTrackIdx = -2;
  
  int hoverCol = -1;
  
  double scaleX = 14.0;
  double scaleY = 0.05;
  int trackHeight = 120;
  int trackStartY = 160;
  
  int selTrack = SEL_NONE;      
  int selStartCol = -1;   
  int selEndCol = -1;     
  bool isSelecting = false;
};

#endif // CONTIGWIDGET_H