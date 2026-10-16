#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QScrollArea>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QLineEdit>
#include <QLabel>
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QCloseEvent>
#include <vector>
#include "contigwidget.h"

class MainWindow : public QMainWindow {
  Q_OBJECT
  
public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override = default;
  
  private slots:
    void openFiles();
  void clearWorkspace();
  void openProject();
  void saveProject();
  void findSequenceDialog();
  void updateVoiceActions();
  void showAboutDialog();
  
protected:
  void closeEvent(QCloseEvent *event) override;
  
private:
  void createActions();
  void createMenus();
  void createToolBars();
  void readSettings();
  void writeSettings();
  
  ContigWidget *contigWidget = nullptr;
  QScrollArea *scrollArea = nullptr;
  
  // Виджеты на тулбарах
  QComboBox *algoCombo = nullptr;
  QDoubleSpinBox *identitySpin = nullptr;
  QComboBox *tableCombo = nullptr;
  QLineEdit *searchEdit = nullptr;
  QComboBox *voiceCombo = nullptr;
  QSlider *speedSlider = nullptr;
  QLabel *speedLabel = nullptr;
  
  // Действия файлов
  QAction *actOpen = nullptr;
  QAction *actClear = nullptr;
  QAction *actOpenProj = nullptr;
  QAction *actSaveProj = nullptr;
  QAction *actExportFasta = nullptr;
  QAction *actExportGenBank = nullptr;
  QAction *actExportImage = nullptr;
  QAction *actPrintExport = nullptr;
  QAction *actExit = nullptr;
  
  // Правка и навигация
  QAction *actUndo = nullptr;
  QAction *actRedo = nullptr;
  QAction *actCopySeq = nullptr;
  QAction *actCopyImg = nullptr;
  QAction *actFind = nullptr;
  QAction *actFindPrev = nullptr;
  QAction *actFindNext = nullptr;
  QAction *actNextSnp = nullptr;
  QAction *actPrevSnp = nullptr;
  
  // Тримминг и анализ
  QAction *actTrimQuality = nullptr;
  QAction *actTrimVectors = nullptr;
  QAction *actBlast = nullptr;
  QAction *actProps = nullptr;
  
  // Масштабирование и отображение
  QAction *actZoomInX = nullptr;
  QAction *actZoomOutX = nullptr;
  QAction *actZoomInY = nullptr;
  QAction *actZoomOutY = nullptr;
  QAction *actTrackHeightUp = nullptr;
  QAction *actTrackHeightDown = nullptr;
  QAction *actResetZoom = nullptr;
  QAction *actToggleOrfA = nullptr;
  QAction *actToggleOrfB = nullptr;
  QAction *actToggleRestr = nullptr;
  QAction *actSelectEnzymes = nullptr;
  QAction *actToggleIupac = nullptr;
  QAction *actToggleAutoRc = nullptr;
  
  QActionGroup *algoGroup = nullptr;
  std::vector<QAction*> algoActions;
  
  // Диктовка и справка
  QAction *actReadAloud = nullptr;
  QAction *actStopRead = nullptr;
  QAction *actAbout = nullptr;
  
  QMenu *menuVoice = nullptr;
  QMenu *menuSpeed = nullptr;
};

#endif // MAINWINDOW_H