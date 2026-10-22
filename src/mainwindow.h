#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QKeySequence>
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
#include <functional>
#include <QStringList>
#include <QCommandLineParser>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QDockWidget>
#include <QTimer>
#include "contigwidget.h"

class MiniMapWidget;

class MainWindow : public QMainWindow {
  Q_OBJECT
  
public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override = default;
  
  // Загрузка файлов секвенирования (запускает выравнивание); возвращает true, если все файлы прочитаны.
  bool loadFiles(const QStringList &files);
  // Применяет параметры командной строки (см. cli.h) и выполняет запрошенные шаги.
  void applyCommandLine(const QCommandLineParser &parser);
  // Предлагает восстановить автосохранённую сессию (после аварийного завершения).
  void maybeRestoreAutosave();
  
private slots:
  void openFiles();
  void loadReference();
  void clearWorkspace();
  void openProject();
  void saveProject();
  void findSequenceDialog();
  void updateVoiceActions();
  void showAboutDialog();
  void saveProjectAs();
  void showCliHelpDialog();
  
protected:
  void closeEvent(QCloseEvent *event) override;
  
private:
  void createActions();
  void createMenus();
  void createToolBars();
  void readSettings();
  void writeSettings();
  void createCoverageDock();
  bool openProjectPath(const QString &path);
  bool doSaveProject(const QString &path);
  bool maybeSave();
  void updateWindowTitle();
  void addRecent(const QString &path);
  void openRecent(const QString &path);
  void updateRecentMenu();
  void clearRecent();
  QString autosavePath() const;
  void reportError(const QString &title, const QString &message);
  void runNextCliStep();
  
  bool loadingSettings = false; // true во время readSettings(): игнорируем writeSettings()
  bool settingsLocked = false;  // параметры командной строки не переписывают сохранённые настройки
  bool batchMode = false;       // --quit: без диалогов, ошибки в stderr
  bool cliFailed = false;
  std::vector<std::function<void()>> cliSteps;
  QString currentProjectPath;
  QTimer *autosaveTimer = nullptr;
  
  QMenu *menuRecent = nullptr;
  QProgressBar *progressBar = nullptr;
  QLabel *progressLabel = nullptr;
  QPushButton *btnCancelAlign = nullptr;
  QDockWidget *coverageDock = nullptr;
  MiniMapWidget *miniMap = nullptr;
  QLabel *statsLabel = nullptr;
  QSpinBox *refineSpin = nullptr;
  QSpinBox *threadsSpin = nullptr;
  QAction *actSaveProjAs = nullptr;
  QAction *actVirtualGel = nullptr;
  QAction *actCliHelp = nullptr;
  
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
  QAction *actLoadRef = nullptr;
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
