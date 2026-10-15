#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QScrollArea>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QAction>
#include <QMenu>
#include "contigwidget.h"

class MainWindow : public QMainWindow {
  Q_OBJECT
  
public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override = default;
  
  private slots:
    void openFiles();
  void openProject();
  void saveProject();
  void findSequenceDialog();
  void updateVoiceActions();
  
private:
  void createActions();
  void createMenus();
  void createToolBars();
  
  ContigWidget *contigWidget = nullptr;
  QScrollArea *scrollArea = nullptr;
  
  QComboBox *voiceCombo = nullptr;
  QSlider *speedSlider = nullptr;
  QLabel *speedLabel = nullptr;
  
  QAction *actOpen = nullptr;
  QAction *actOpenProj = nullptr;
  QAction *actSaveProj = nullptr;
  QAction *actExportFasta = nullptr;
  QAction *actExportGenBank = nullptr;
  QAction *actExportImage = nullptr;
  QAction *actPrintExport = nullptr;
  QAction *actExit = nullptr;
  
  QAction *actUndo = nullptr;
  QAction *actRedo = nullptr;
  QAction *actCopySeq = nullptr;
  QAction *actCopyImg = nullptr;
  QAction *actFind = nullptr;
  QAction *actTrimQuality = nullptr;
  QAction *actTrimVectors = nullptr;
  
  QAction *actZoomInX = nullptr;
  QAction *actZoomOutX = nullptr;
  QAction *actZoomInY = nullptr;
  QAction *actZoomOutY = nullptr;
  QAction *actResetZoom = nullptr;
  QAction *actToggleOrfA = nullptr;
  QAction *actToggleOrfB = nullptr;
  QAction *actToggleRestr = nullptr;
  
  QAction *actBlast = nullptr;
  QAction *actProps = nullptr;
  QAction *actTm = nullptr;
  QAction *actNextSnp = nullptr;
  QAction *actPrevSnp = nullptr;
  
  QAction *actReadAloud = nullptr;
  QAction *actStopRead = nullptr;
  
  QMenu *menuVoice = nullptr;
  QMenu *menuSpeed = nullptr;
};

#endif // MAINWINDOW_H