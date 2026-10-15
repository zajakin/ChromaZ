#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QScrollArea>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QLabel>
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QCloseEvent>
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
  
  // Действия правки и навигации
  QAction *actUndo = nullptr;
  QAction *actRedo = nullptr;
  QAction *actCopySeq = nullptr;
  QAction *actCopyImg = nullptr;
  QAction *actFind = nullptr;
  QAction *actNextSnp = nullptr;
  QAction *actPrevSnp = nullptr;
  
  // Действия тримминга и биологического анализа
  QAction *actTrimQuality = nullptr;
  QAction *actTrimVectors = nullptr;
  QAction *actBlast = nullptr;
  QAction *actProps = nullptr;
  
  // Масштабирование и отображение
  QAction *actZoomInX = nullptr;
  QAction *actZoomOutX = nullptr;
  QAction *actZoomInY = nullptr;
  QAction *actZoomOutY = nullptr;
  QAction *actResetZoom = nullptr;
  QAction *actToggleOrfA = nullptr;
  QAction *actToggleOrfB = nullptr;
  QAction *actToggleRestr = nullptr;
  QAction *actToggleIupac = nullptr;
  QAction *actToggleAutoRc = nullptr;
  
  QActionGroup *algoGroup = nullptr;
  std::vector<QAction*> algoActions;
  
  // Голосовое чтение и справка
  QAction *actReadAloud = nullptr;
  QAction *actStopRead = nullptr;
  QAction *actAbout = nullptr;
  
  QMenu *menuVoice = nullptr;
  QMenu *menuSpeed = nullptr;
};

#endif // MAINWINDOW_H