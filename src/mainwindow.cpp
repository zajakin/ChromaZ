#include "mainwindow.h"
#include <QMenuBar>
#include <QToolBar>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QApplication>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QLineEdit>
#include <QPushButton>
#include <QLocale>
#include <QSettings>
#include <QSignalBlocker>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QDateTime>
#include <QDialog>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QStatusBar>
#include <cmath>
#include <cstdio>
#include "minimapwidget.h"
#include "cli.h"

#ifndef CHROMAZ_VERSION
#define CHROMAZ_VERSION "1.0.0"
#endif

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle("ChromaZ — Sanger Alignment & Contig Editor");
  resize(1400, 850);
  
  contigWidget = new ContigWidget(this);
  scrollArea = new QScrollArea(this);
  scrollArea->setWidget(contigWidget);
  scrollArea->setWidgetResizable(false);
  setCentralWidget(scrollArea);
  
  createActions();
  createCoverageDock();
  createMenus();
  createToolBars();
  
  connect(contigWidget, &ContigWidget::restrictionSitesToggled, actToggleRestr, &QAction::setChecked);
  connect(contigWidget, &ContigWidget::autoRCChanged, actToggleAutoRc, &QAction::setChecked);
  connect(contigWidget, &ContigWidget::showOrfAChanged, actToggleOrfA, &QAction::setChecked);
  connect(contigWidget, &ContigWidget::showOrfBChanged, actToggleOrfB, &QAction::setChecked);
  connect(contigWidget, &ContigWidget::consensusIupacChanged, actToggleIupac, &QAction::setChecked);
  connect(contigWidget, &ContigWidget::alignmentAlgorithmChanged, this, [this](int index) {
    if (algoCombo) { QSignalBlocker blocker(algoCombo); algoCombo->setCurrentIndex(index); }
    if (index >= 0 && index < static_cast<int>(algoActions.size())) {
      QSignalBlocker blocker(algoActions[static_cast<size_t>(index)]);
      algoActions[static_cast<size_t>(index)]->setChecked(true);
    }
  });
  connect(contigWidget, &ContigWidget::translationTableChanged, this, [this](int index) {
    if (tableCombo) { QSignalBlocker blocker(tableCombo); tableCombo->setCurrentIndex(index); }
  });
  connect(contigWidget, &ContigWidget::minIdentityThresholdChanged, this, [this](double threshold) {
    if (identitySpin) { QSignalBlocker blocker(identitySpin); identitySpin->setValue(threshold); }
  });
  connect(contigWidget, &ContigWidget::speechRateChanged, this, [this](double rate) {
    if (speedSlider) {
      int val = std::round((rate + 1.0) * 50.0);
      if (speedSlider->value() != val) speedSlider->setValue(val);
    }
  });
  
  // Индикатор фонового выравнивания в строке состояния
  progressLabel = new QLabel(this);
  progressBar = new QProgressBar(this);
  progressBar->setMaximumWidth(220);
  progressBar->setTextVisible(true);
  btnCancelAlign = new QPushButton("Cancel", this);
  statusBar()->addPermanentWidget(progressLabel);
  statusBar()->addPermanentWidget(progressBar);
  statusBar()->addPermanentWidget(btnCancelAlign);
  progressLabel->hide(); progressBar->hide(); btnCancelAlign->hide();
  connect(btnCancelAlign, &QPushButton::clicked, contigWidget, &ContigWidget::cancelAlignment);
  connect(contigWidget, &ContigWidget::alignmentStarted, this, [this]() {
    progressLabel->setText("Aligning…");
    progressBar->setRange(0, 0);   // "занят", пока не пришёл первый прогресс
    progressLabel->show(); progressBar->show(); btnCancelAlign->show();
  });
  connect(contigWidget, &ContigWidget::alignmentProgress, this, [this](int done, int total, const QString &stage) {
    if (total > 0) { progressBar->setRange(0, total); progressBar->setValue(done); }
    progressLabel->setText(stage);
  });
  connect(contigWidget, &ContigWidget::alignmentFinished, this, [this](bool applied) {
    progressLabel->hide(); progressBar->hide(); btnCancelAlign->hide();
    statusBar()->showMessage(applied ? "Alignment complete" : "Alignment cancelled", 4000);
    updateWindowTitle();
    QTimer::singleShot(0, this, [this]() { runNextCliStep(); });
  });
  connect(contigWidget, &ContigWidget::modifiedChanged, this, [this](bool) { updateWindowTitle(); });
  connect(contigWidget, &ContigWidget::projectLoaded, this, [this](const QString &path) {
    if (path == autosavePath()) { currentProjectPath.clear(); }
    else { currentProjectPath = path; addRecent(path); }
    updateWindowTitle();
  });

  // Автосохранение сессии (раз в 2 минуты при наличии несохранённых изменений)
  autosaveTimer = new QTimer(this);
  autosaveTimer->setInterval(120000);
  connect(autosaveTimer, &QTimer::timeout, this, [this]() {
    if (batchMode || !contigWidget->hasTracks() || !contigWidget->isModified()) return;
    if (contigWidget->saveProject(autosavePath())) statusBar()->showMessage("Session autosaved", 2000);
  });
  autosaveTimer->start();

  readSettings();
  updateWindowTitle();
}

void MainWindow::closeEvent(QCloseEvent *event) {
  if (!maybeSave()) { event->ignore(); return; }
  writeSettings();
  QFile::remove(autosavePath());   // чистое завершение — автосохранение больше не нужно
  QMainWindow::closeEvent(event);
}

void MainWindow::writeSettings() {
  if (loadingSettings || settingsLocked) return;
  QSettings settings("ChromaZ", "ChromaZ");
  settings.setValue("geometry", saveGeometry());
  settings.setValue("windowState", saveState());
  settings.setValue("speechRate", contigWidget->getSpeechRate());
  
  QString vName = contigWidget->getCurrentVoice().name();
  if (!vName.isEmpty()) {
    settings.setValue("speechVoice", vName);
  }
  
  settings.setValue("alignmentAlgorithm", static_cast<int>(contigWidget->getAlignmentAlgorithm()));
  if (identitySpin) {
    settings.setValue("minIdentityThreshold", identitySpin->value());
  }
  settings.setValue("translationTable", contigWidget->getTranslationTable());
  settings.setValue("autoRC", contigWidget->isAutoRCEnabled());
  settings.setValue("orfA", contigWidget->isOrfAEnabled());
  settings.setValue("orfB", contigWidget->isOrfBEnabled());
  settings.setValue("restrictionSites", contigWidget->isRestrictionSitesVisible());
  settings.setValue("iupacConsensus", contigWidget->isIupacConsensusEnabled());
  settings.setValue("refineIterations", contigWidget->getRefineIterations());
  settings.setValue("alignThreads", contigWidget->getThreadCount());
}

void MainWindow::readSettings() {
  // Каждое восстановленное значение переключает QAction/QComboBox, а их обработчики вызывают
  // writeSettings() — без этого флага ещё не прочитанные ключи затирались бы значениями по умолчанию.
  loadingSettings = true;
  QSettings settings("ChromaZ", "ChromaZ");
  if (settings.contains("geometry")) {
    restoreGeometry(settings.value("geometry").toByteArray());
  }
  if (settings.contains("windowState")) {
    restoreState(settings.value("windowState").toByteArray());
  }
  
  if (settings.contains("speechRate")) {
    double rate = settings.value("speechRate", 0.0).toDouble();
    contigWidget->setSpeechRate(rate);
    if (speedSlider) {
      int val = std::round((rate + 1.0) * 50.0);
      speedSlider->setValue(val);
    }
  }
  
  if (settings.contains("speechVoice")) {
    QString vName = settings.value("speechVoice").toString();
    for (const auto &v : contigWidget->getAvailableVoices()) {
      if (v.name() == vName) {
        contigWidget->setVoice(v);
        if (voiceCombo) {
          int idx = voiceCombo->findData(v.name());
          if (idx >= 0) voiceCombo->setCurrentIndex(idx);
        }
        break;
      }
    }
  }
  
  if (settings.contains("alignmentAlgorithm")) {
    int algo = settings.value("alignmentAlgorithm", 0).toInt();
    contigWidget->setAlignmentAlgorithm(algo);
    if (algoCombo && algo >= 0 && algo < algoCombo->count()) {
      algoCombo->setCurrentIndex(algo);
    }
    if (algo >= 0 && algo < static_cast<int>(algoActions.size())) {
      algoActions[algo]->setChecked(true);
    }
  }
  
  if (settings.contains("minIdentityThreshold")) {
    double val = settings.value("minIdentityThreshold", 45.0).toDouble();
    contigWidget->setMinIdentityThreshold(val);
    if (identitySpin) {
      identitySpin->setValue(val);
    }
  }
  
  if (settings.contains("translationTable")) {
    int tableIdx = settings.value("translationTable", 0).toInt();
    contigWidget->setTranslationTable(tableIdx);
    if (tableCombo && tableIdx >= 0 && tableIdx < tableCombo->count()) {
      tableCombo->setCurrentIndex(tableIdx);
    }
  }
  
  if (settings.contains("autoRC")) {
    bool v = settings.value("autoRC", true).toBool();
    contigWidget->setAutoRC(v);
    if (actToggleAutoRc) actToggleAutoRc->setChecked(v);
  }
  
  if (settings.contains("orfA")) {
    bool v = settings.value("orfA", true).toBool();
    contigWidget->setShowOrfA(v);
    if (actToggleOrfA) actToggleOrfA->setChecked(v);
  }
  
  if (settings.contains("orfB")) {
    bool v = settings.value("orfB", true).toBool();
    contigWidget->setShowOrfB(v);
    if (actToggleOrfB) actToggleOrfB->setChecked(v);
  }
  
  if (settings.contains("restrictionSites")) {
    bool v = settings.value("restrictionSites", false).toBool();
    contigWidget->toggleRestrictionSites(v);
    if (actToggleRestr) actToggleRestr->setChecked(v);
  }
  
  if (settings.contains("iupacConsensus")) {
    bool v = settings.value("iupacConsensus", false).toBool();
    contigWidget->setConsensusIupac(v);
    if (actToggleIupac) actToggleIupac->setChecked(v);
  }
  if (settings.contains("refineIterations")) {
    int v = settings.value("refineIterations", 0).toInt();
    if (refineSpin) refineSpin->setValue(v);
    contigWidget->setRefineIterations(v);
  }
  if (settings.contains("alignThreads")) {
    int v = settings.value("alignThreads", 0).toInt();
    if (threadsSpin) threadsSpin->setValue(v);
    contigWidget->setThreadCount(v);
  }
  loadingSettings = false;
}

void MainWindow::createActions() {
  actOpen = new QAction("📂 Open Sequencing Files...", this);
  actOpen->setShortcut(QKeySequence::Open);
  connect(actOpen, &QAction::triggered, this, &MainWindow::openFiles);
  
  actClear = new QAction("🗑 Clear Workspace", this);
  actClear->setToolTip("Remove all loaded tracks and sequences, reset workspace");
  connect(actClear, &QAction::triggered, this, &MainWindow::clearWorkspace);
  
  actOpenProj = new QAction("📁 Open Project (.ChromaZ)...", this);
  connect(actOpenProj, &QAction::triggered, this, &MainWindow::openProject);
  
  actSaveProj = new QAction("💾 Save Project (.ChromaZ)", this);
  actSaveProj->setShortcut(QKeySequence::Save);
  connect(actSaveProj, &QAction::triggered, this, &MainWindow::saveProject);
  
  actExportFasta = new QAction("📄 Export Consensus FASTA...", this);
  connect(actExportFasta, &QAction::triggered, this, [this]() {
    QString p = QFileDialog::getSaveFileName(this, "Export Consensus FASTA", "consensus.fasta", "FASTA (*.fasta *.fa)");
    if (!p.isEmpty()) contigWidget->exportConsensusFasta(p);
  });
  
  actExportGenBank = new QAction("📥 Export Annotated GenBank (.gbk)...", this);
  connect(actExportGenBank, &QAction::triggered, contigWidget, &ContigWidget::exportConsensusGenBankPrompt);
  
  actExportImage = new QAction("🖼 Export View PNG...", this);
  connect(actExportImage, &QAction::triggered, this, [this]() {
    QString p = QFileDialog::getSaveFileName(this, "Export View Image", "alignment.png", "PNG (*.png)");
    if (!p.isEmpty()) contigWidget->exportViewImage(p);
  });
  
  actPrintExport = new QAction("🖨 Print & Export PDF Report...", this);
  actPrintExport->setShortcut(QKeySequence::Print);
  connect(actPrintExport, &QAction::triggered, contigWidget, &ContigWidget::showPrintExportDialog);
  
  actExit = new QAction("❌ Exit", this);
  connect(actExit, &QAction::triggered, this, &QWidget::close);
  
  actUndo = new QAction("↩ Undo", this);
  actUndo->setShortcut(QKeySequence::Undo);
  connect(actUndo, &QAction::triggered, contigWidget, &ContigWidget::undo);
  
  actRedo = new QAction("↪ Redo", this);
  actRedo->setShortcut(QKeySequence::Redo);
  connect(actRedo, &QAction::triggered, contigWidget, &ContigWidget::redo);
  
  actCopySeq = new QAction("📋 Copy Sequence", this);
  actCopySeq->setShortcut(QKeySequence::Copy);
  connect(actCopySeq, &QAction::triggered, contigWidget, &ContigWidget::copySelectedToClipboard);
  
  actCopyImg = new QAction("🖼 Copy Selection as Image", this);
  connect(actCopyImg, &QAction::triggered, contigWidget, &ContigWidget::copySelectionAsImageToClipboard);
  
  actFind = new QAction("🔍 Find Sequence...", this);
  actFind->setShortcut(QKeySequence::Find);
  connect(actFind, &QAction::triggered, this, &MainWindow::findSequenceDialog);
  
  actFindPrev = new QAction("◀ Find", this);
  actFindPrev->setToolTip("Find previous occurrence");
  connect(actFindPrev, &QAction::triggered, this, [this]() {
    if (searchEdit && !searchEdit->text().isEmpty()) {
      contigWidget->findSequence(searchEdit->text(), false);
    }
  });
  
  actFindNext = new QAction("Find ▶", this);
  actFindNext->setToolTip("Find next occurrence");
  connect(actFindNext, &QAction::triggered, this, [this]() {
    if (searchEdit && !searchEdit->text().isEmpty()) {
      contigWidget->findSequence(searchEdit->text(), true);
    }
  });
  
  actNextSnp = new QAction("SNP ▶", this);
  actNextSnp->setToolTip("Jump to next Single Nucleotide Polymorphism");
  connect(actNextSnp, &QAction::triggered, contigWidget, &ContigWidget::jumpNextSNP);
  
  actPrevSnp = new QAction("◀ SNP", this);
  actPrevSnp->setToolTip("Jump to previous Single Nucleotide Polymorphism");
  connect(actPrevSnp, &QAction::triggered, contigWidget, &ContigWidget::jumpPrevSNP);
  
  actTrimQuality = new QAction("✂ Trim Low Quality...", this);
  actTrimQuality->setToolTip("Trim read ends based on Phred Quality score");
  connect(actTrimQuality, &QAction::triggered, this, [this]() {
    bool ok;
    int q = QInputDialog::getInt(this, "Quality Trimming", "Minimum Phred Quality score:", 20, 5, 50, 1, &ok);
    if (ok) contigWidget->trimLowQualityEnds(q);
  });
  
  actTrimVectors = new QAction("✂ Trim Vectors", this);
  actTrimVectors->setToolTip("Trim vector backbones and adapters (UniVec)");
  connect(actTrimVectors, &QAction::triggered, contigWidget, &ContigWidget::trimVectors);
  
  actZoomInX = new QAction("🔍+ Bases", this);
  actZoomInX->setToolTip("Increase nucleotide base width spacing");
  connect(actZoomInX, &QAction::triggered, contigWidget, &ContigWidget::zoomInX);
  
  actZoomOutX = new QAction("🔍- Bases", this);
  actZoomOutX->setToolTip("Decrease nucleotide base width spacing");
  connect(actZoomOutX, &QAction::triggered, contigWidget, &ContigWidget::zoomOutX);
  
  actZoomInY = new QAction("📈+ Peaks", this);
  actZoomInY->setToolTip("Increase chromatogram peak height amplitude");
  connect(actZoomInY, &QAction::triggered, contigWidget, &ContigWidget::zoomInY);
  
  actZoomOutY = new QAction("📉- Peaks", this);
  actZoomOutY->setToolTip("Decrease chromatogram peak height amplitude");
  connect(actZoomOutY, &QAction::triggered, contigWidget, &ContigWidget::zoomOutY);
  
  actTrackHeightUp = new QAction("↕+ Row Height", this);
  actTrackHeightUp->setToolTip("Increase the vertical row height of each track");
  connect(actTrackHeightUp, &QAction::triggered, contigWidget, &ContigWidget::increaseTrackHeight);
  
  actTrackHeightDown = new QAction("↕- Row Height", this);
  actTrackHeightDown->setToolTip("Decrease the vertical row height of each track");
  connect(actTrackHeightDown, &QAction::triggered, contigWidget, &ContigWidget::decreaseTrackHeight);
  
  actResetZoom = new QAction("🔄 Reset Zoom", this);
  actResetZoom->setToolTip("Reset peak height, row height, and horizontal spacing to defaults");
  connect(actResetZoom, &QAction::triggered, contigWidget, &ContigWidget::resetZoom);
  
  actToggleOrfA = new QAction("ORF A (Ref)", this);
  actToggleOrfA->setCheckable(true);
  actToggleOrfA->setChecked(true);
  connect(actToggleOrfA, &QAction::toggled, this, [this](bool val) {
    contigWidget->setShowOrfA(val);
    writeSettings();
  });
  
  actToggleOrfB = new QAction("ORF B (Cons)", this);
  actToggleOrfB->setCheckable(true);
  actToggleOrfB->setChecked(true);
  connect(actToggleOrfB, &QAction::toggled, this, [this](bool val) {
    contigWidget->setShowOrfB(val);
    writeSettings();
  });
  
  actToggleRestr = new QAction("✂ Restriction Sites", this);
  actToggleRestr->setCheckable(true);
  actToggleRestr->setChecked(false);
  connect(actToggleRestr, &QAction::toggled, this, [this](bool val) {
    contigWidget->toggleRestrictionSites(val);
    writeSettings();
  });
  
  actSelectEnzymes = new QAction("⚙ Select Enzymes...", this);
  actSelectEnzymes->setToolTip("Choose which restriction enzymes to show or hide");
  connect(actSelectEnzymes, &QAction::triggered, contigWidget, &ContigWidget::showEnzymeSelectionDialog);
  
  actToggleIupac = new QAction("IUPAC Ambiguity", this);
  actToggleIupac->setCheckable(true);
  actToggleIupac->setChecked(false);
  connect(actToggleIupac, &QAction::toggled, this, [this](bool val) {
    contigWidget->setConsensusIupac(val);
    writeSettings();
  });
  
  actToggleAutoRc = new QAction("🔄 Auto RC", this);
  actToggleAutoRc->setToolTip("Automatically orient complementary reads to match reference strand");
  actToggleAutoRc->setCheckable(true);
  actToggleAutoRc->setChecked(true);
  connect(actToggleAutoRc, &QAction::toggled, this, [this](bool val) {
    contigWidget->setAutoRC(val);
    writeSettings();
  });
  
  algoGroup = new QActionGroup(this);
  const QStringList algoNames = {
    "OLC / CAP3 (Sanger Default)",
    "Affine-Gap Block (Gotoh)",
    "Needleman-Wunsch (Global)",
    "Smith-Waterman (Local)",
    "BLAST (Seed & Extend)",
    "Banded DP (Global)",
    "Wavefront (WFA)",
    "Internal Clustal-style"
  };
  for (int i = 0; i < algoNames.size(); ++i) {
    QAction *a = new QAction(algoNames[i], algoGroup);
    a->setCheckable(true);
    if (i == 0) a->setChecked(true);
    connect(a, &QAction::triggered, this, [this, i]() {
      contigWidget->setAlignmentAlgorithm(i);
      if (algoCombo && algoCombo->currentIndex() != i) {
        algoCombo->setCurrentIndex(i);
      }
      writeSettings();
    });
    algoActions.push_back(a);
  }
  
  actBlast = new QAction("🌐 BLASTn", this);
  actBlast->setToolTip("Send selected region to NCBI BLASTn in web browser");
  connect(actBlast, &QAction::triggered, contigWidget, &ContigWidget::openInNcbiBlast);
  
  actProps = new QAction("📊 Properties", this);
  actProps->setToolTip("Show GC Profile, estimated Tm, MW, and Kyte-Doolittle Hydrophobicity");
  connect(actProps, &QAction::triggered, contigWidget, &ContigWidget::showSequenceProperties);
  
  actReadAloud = new QAction("🔊 Read Aloud", this);
  connect(actReadAloud, &QAction::triggered, contigWidget, &ContigWidget::readSelectionAloud);
  
  actStopRead = new QAction("🔇 Stop", this);
  connect(actStopRead, &QAction::triggered, contigWidget, &ContigWidget::stopReading);
  
  actSaveProjAs = new QAction("💾 Save Project As...", this);
  actSaveProjAs->setShortcut(QKeySequence::SaveAs);
  connect(actSaveProjAs, &QAction::triggered, this, &MainWindow::saveProjectAs);
  
  actVirtualGel = new QAction("🧫 Virtual Gel...", this);
  actVirtualGel->setToolTip("Simulate an agarose gel of the consensus digested with the selected restriction enzymes");
  connect(actVirtualGel, &QAction::triggered, contigWidget, &ContigWidget::showVirtualGelDialog);
  
  actCliHelp = new QAction("⌨ Command-line options...", this);
  connect(actCliHelp, &QAction::triggered, this, &MainWindow::showCliHelpDialog);
  
  actAbout = new QAction("ℹ About ChromaZ...", this);
  connect(actAbout, &QAction::triggered, this, &MainWindow::showAboutDialog);
}

void MainWindow::createMenus() {
  QMenu *fileMenu = menuBar()->addMenu("&File");
  fileMenu->addAction(actOpen);
  menuRecent = fileMenu->addMenu("🕘 Open Recent");
  connect(menuRecent, &QMenu::aboutToShow, this, &MainWindow::updateRecentMenu);
  fileMenu->addAction(actClear);
  fileMenu->addSeparator();
  fileMenu->addAction(actOpenProj);
  fileMenu->addAction(actSaveProj);
  fileMenu->addAction(actSaveProjAs);
  fileMenu->addSeparator();
  fileMenu->addAction(actExportFasta);
  fileMenu->addAction(actExportGenBank);
  fileMenu->addAction(actExportImage);
  fileMenu->addSeparator();
  fileMenu->addAction(actPrintExport);
  fileMenu->addSeparator();
  fileMenu->addAction(actExit);
  
  QMenu *editMenu = menuBar()->addMenu("&Edit");
  editMenu->addAction(actUndo);
  editMenu->addAction(actRedo);
  editMenu->addSeparator();
  editMenu->addAction(actCopySeq);
  editMenu->addAction(actCopyImg);
  editMenu->addSeparator();
  editMenu->addAction(actFind);
  editMenu->addAction(actFindPrev);
  editMenu->addAction(actFindNext);
  editMenu->addSeparator();
  editMenu->addAction(actTrimQuality);
  editMenu->addAction(actTrimVectors);
  
  QMenu *viewMenu = menuBar()->addMenu("&View");
  viewMenu->addAction(actZoomInY);
  viewMenu->addAction(actZoomOutY);
  viewMenu->addAction(actTrackHeightUp);
  viewMenu->addAction(actTrackHeightDown);
  viewMenu->addAction(actZoomInX);
  viewMenu->addAction(actZoomOutX);
  viewMenu->addAction(actResetZoom);
  viewMenu->addSeparator();
  viewMenu->addAction(actToggleOrfA);
  viewMenu->addAction(actToggleOrfB);
  viewMenu->addAction(actToggleRestr);
  viewMenu->addAction(actSelectEnzymes);
  viewMenu->addAction(actToggleIupac);
  viewMenu->addSeparator();
  viewMenu->addAction(coverageDock->toggleViewAction());
  
  QMenu *alignMenu = menuBar()->addMenu("&Alignment");
  QMenu *algoSub = alignMenu->addMenu("🧬 Algorithm");
  for (QAction *a : algoActions) {
    algoSub->addAction(a);
  }
  alignMenu->addAction(actToggleAutoRc);
  
  QMenu *analysisMenu = menuBar()->addMenu("&Analysis");
  analysisMenu->addAction(actSelectEnzymes);
  analysisMenu->addAction(actBlast);
  analysisMenu->addAction(actProps);
  analysisMenu->addAction(actVirtualGel);
  analysisMenu->addSeparator();
  analysisMenu->addAction(actPrevSnp);
  analysisMenu->addAction(actNextSnp);
  
  QMenu *speechMenu = menuBar()->addMenu("&Speech");
  speechMenu->addAction(actReadAloud);
  speechMenu->addAction(actStopRead);
  speechMenu->addSeparator();
  
  menuVoice = speechMenu->addMenu("🎙 Select Voice");
  connect(menuVoice, &QMenu::aboutToShow, this, &MainWindow::updateVoiceActions);
  
  menuSpeed = speechMenu->addMenu("⏱ Speed Presets");
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
    QAction *spAct = menuSpeed->addAction(sp.label);
    connect(spAct, &QAction::triggered, this, [this, r = sp.rate]() {
      contigWidget->setSpeechRate(r);
      writeSettings();
    });
  }
  
  QMenu *helpMenu = menuBar()->addMenu("&Help");
  helpMenu->addAction(actCliHelp);
  helpMenu->addSeparator();
  helpMenu->addAction(actAbout);
}

void MainWindow::updateVoiceActions() {
  menuVoice->clear();
  const auto voices = contigWidget->getAvailableVoices();
  QVoice curVoice = contigWidget->getCurrentVoice();
  
  for (const QVoice &v : voices) {
    QString label = QString("%1 (%2)").arg(v.name()).arg(QLocale::languageToString(v.locale().language()));
    QAction *vAct = menuVoice->addAction(label);
    vAct->setCheckable(true);
    if (v.name() == curVoice.name()) vAct->setChecked(true);
    connect(vAct, &QAction::triggered, this, [this, v]() {
      contigWidget->setVoice(v);
      if (voiceCombo) {
        int idx = voiceCombo->findData(v.name());
        if (idx >= 0) voiceCombo->setCurrentIndex(idx);
      }
      writeSettings();
    });
  }
}

void MainWindow::createToolBars() {
  // 1. Панель файлов и проекта
  QToolBar *fileBar = addToolBar("File & Project");
  fileBar->setObjectName("FileToolBar");
  fileBar->addAction(actOpen);
  fileBar->addAction(actClear);
  fileBar->addAction(actSaveProj);
  fileBar->addAction(actPrintExport);
  
  // 2. Панель выравнивания и параметров сборки
  QToolBar *alignBar = addToolBar("Alignment & Assembly");
  alignBar->setObjectName("AlignToolBar");
  
  alignBar->addWidget(new QLabel(" Algo: ", this));
  algoCombo = new QComboBox(this);
  algoCombo->setToolTip("Select pairwise alignment algorithm");
  const QStringList algoNames = {
    "CAP3 (OLC)",
    "Gotoh (Affine)",
    "Needleman-Wunsch",
    "Smith-Waterman",
    "BLAST (Seed)",
    "Banded DP",
    "Wavefront (WFA)",
    "Clustal-style"
  };
  algoCombo->addItems(algoNames);
  alignBar->addWidget(algoCombo);
  
  connect(algoCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
    if (idx < 0) return;
    contigWidget->setAlignmentAlgorithm(idx);
    if (idx >= 0 && idx < static_cast<int>(algoActions.size())) {
      algoActions[idx]->setChecked(true);
    }
    writeSettings();
  });
  
  alignBar->addWidget(new QLabel(" Min Id%: ", this));
  identitySpin = new QDoubleSpinBox(this);
  identitySpin->setToolTip("Minimum identity threshold to include read into contig");
  identitySpin->setRange(0.0, 100.0);
  identitySpin->setDecimals(1);
  identitySpin->setSingleStep(1.0);
  identitySpin->setValue(45.0);
  identitySpin->setSuffix("%");
  alignBar->addWidget(identitySpin);
  
  connect(identitySpin, &QDoubleSpinBox::valueChanged, this, [this](double val) {
    contigWidget->setMinIdentityThreshold(val);
    writeSettings();
  });
  
  alignBar->addAction(actToggleAutoRc);
  
  alignBar->addWidget(new QLabel(" Code: ", this));
  tableCombo = new QComboBox(this);
  tableCombo->setToolTip("Genetic Code Translation Table");
  tableCombo->addItem("Standard");
  tableCombo->addItem("Vertebrate Mito");
  tableCombo->addItem("Bacterial/Plastid");
  alignBar->addWidget(tableCombo);
  
  connect(tableCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
    if (idx < 0) return;
    contigWidget->setTranslationTable(idx);
    writeSettings();
  });
  
  alignBar->addWidget(new QLabel(" Refine: ", this));
  refineSpin = new QSpinBox(this);
  refineSpin->setRange(0, 10);
  refineSpin->setToolTip("Consensus refinement rounds: re-align all reads to the consensus built from the previous round (0 = off)");
  alignBar->addWidget(refineSpin);
  connect(refineSpin, &QSpinBox::valueChanged, this, [this](int v) {
    contigWidget->setRefineIterations(v);
    writeSettings();
  });
  
  alignBar->addWidget(new QLabel(" Threads: ", this));
  threadsSpin = new QSpinBox(this);
  threadsSpin->setRange(0, 64);
  threadsSpin->setSpecialValueText("auto");
  threadsSpin->setToolTip("Worker threads for read alignment (auto = number of CPU cores)");
  alignBar->addWidget(threadsSpin);
  connect(threadsSpin, &QSpinBox::valueChanged, this, [this](int v) {
    contigWidget->setThreadCount(v);
    writeSettings();
  });
  
  // 3. Панель масштабирования и отображения
  QToolBar *viewBar = addToolBar("View & Zoom");
  viewBar->setObjectName("ViewToolBar");
  viewBar->addAction(actZoomInY);
  viewBar->addAction(actZoomOutY);
  viewBar->addAction(actTrackHeightUp);
  viewBar->addAction(actTrackHeightDown);
  viewBar->addAction(actZoomInX);
  viewBar->addAction(actZoomOutX);
  viewBar->addAction(actResetZoom);
  viewBar->addSeparator();
  viewBar->addAction(actToggleOrfA);
  viewBar->addAction(actToggleOrfB);
  viewBar->addAction(actToggleRestr);
  viewBar->addAction(actToggleIupac);
  
  // 4. Панель редактирования, поиска и SNP
  QToolBar *editBar = addToolBar("Edit & Navigation");
  editBar->setObjectName("EditToolBar");
  editBar->addAction(actUndo);
  editBar->addAction(actRedo);
  editBar->addSeparator();
  
  searchEdit = new QLineEdit(this);
  searchEdit->setPlaceholderText("Find bases...");
  searchEdit->setMaximumWidth(120);
  searchEdit->setClearButtonEnabled(true);
  connect(searchEdit, &QLineEdit::returnPressed, this, [this]() {
    if (searchEdit && !searchEdit->text().isEmpty()) {
      contigWidget->findSequence(searchEdit->text(), true);
    }
  });
  editBar->addWidget(searchEdit);
  editBar->addAction(actFindPrev);
  editBar->addAction(actFindNext);
  editBar->addSeparator();
  editBar->addAction(actPrevSnp);
  editBar->addAction(actNextSnp);
  
  // 5. Панель анализа и тримминга
  QToolBar *analysisBar = addToolBar("Analysis & Tools");
  analysisBar->setObjectName("AnalysisToolBar");
  analysisBar->addAction(actSelectEnzymes);
  analysisBar->addAction(actTrimVectors);
  analysisBar->addAction(actTrimQuality);
  analysisBar->addSeparator();
  analysisBar->addAction(actBlast);
  analysisBar->addAction(actProps);
  analysisBar->addAction(actVirtualGel);
  
  // 6. Панель голосовой диктовки
  QToolBar *speechBar = addToolBar("Text-to-Speech Controls");
  speechBar->setObjectName("SpeechToolBar");
  speechBar->addAction(actReadAloud);
  speechBar->addAction(actStopRead);
  
  voiceCombo = new QComboBox(this);
  voiceCombo->setMinimumWidth(150);
  speechBar->addWidget(new QLabel(" 🎙 ", this));
  speechBar->addWidget(voiceCombo);
  
  auto populateVoiceCombo = [this]() {
    voiceCombo->clear();
    const auto voices = contigWidget->getAvailableVoices();
    QVoice cur = contigWidget->getCurrentVoice();
    int curIdx = 0;
    for (int i = 0; i < voices.size(); ++i) {
      voiceCombo->addItem(voices[i].name(), voices[i].name());
      if (voices[i].name() == cur.name()) curIdx = i;
    }
    if (!voices.empty()) voiceCombo->setCurrentIndex(curIdx);
  };
  populateVoiceCombo();
  
  connect(voiceCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
    if (idx < 0) return;
    const auto voices = contigWidget->getAvailableVoices();
    if (idx < voices.size()) {
      contigWidget->setVoice(voices[idx]);
      writeSettings();
    }
  });
  
  speechBar->addWidget(new QLabel(" ⏱ ", this));
  speedSlider = new QSlider(Qt::Horizontal, this);
  speedSlider->setRange(0, 100);
  speedSlider->setValue(50);
  speedSlider->setMaximumWidth(90);
  speechBar->addWidget(speedSlider);
  
  speedLabel = new QLabel(" 1.0x ", this);
  speechBar->addWidget(speedLabel);
  
  connect(speedSlider, &QSlider::valueChanged, this, [this](int val) {
    double rate = (val - 50.0) / 50.0;
    double multiplier = (rate >= 0.0) ? (1.0 + rate) : (1.0 + rate * 0.5);
    speedLabel->setText(QString(" %1x ").arg(multiplier, 0, 'f', 2));
    contigWidget->setSpeechRate(rate);
  });
}

void MainWindow::clearWorkspace() {
  if (QMessageBox::question(this, "Clear Workspace",
                            "Are you sure you want to remove all loaded tracks and reset the alignment view?",
                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
    contigWidget->clearAll();
  }
}

void MainWindow::showAboutDialog() {
  QMessageBox::about(this, "About ChromaZ",
                     QString("<h3>ChromaZ v%1</h3>"
                               "<p><b>Sanger Chromatogram Alignment & Contig Editor</b></p>"
                               "<p>High-performance cross-platform application for Sanger trace inspection, "
                               "contig assembly, restriction mapping, and sequence analysis.</p>"
                               "<p><b>Author:</b> Pawel Zayakin (&lt;pz@magisters.net&gt;)</p>"
                               "<p><b>GitHub:</b> <a href=\"https://github.com/zajakin/ChromaZ\">https://github.com/zajakin/ChromaZ</a></p>"
                               "<p>Released under the MIT License.</p>")
                                                                 .arg(CHROMAZ_VERSION));
}

void MainWindow::reportError(const QString &title, const QString &message) {
  if (batchMode) {
    std::fprintf(stderr, "ChromaZ: %s: %s\n", title.toLocal8Bit().constData(), message.toLocal8Bit().constData());
    cliFailed = true;
  } else {
    QMessageBox::warning(this, title, message);
  }
}

void MainWindow::openFiles() {
  QStringList files = QFileDialog::getOpenFileNames(
    this, "Open Sequencing / Alignment Files", "",
    "All Supported Files (*.ab1 *.scf *.fasta *.fa *.gb *.gbk *.fastq *.fq *.txt);;AB1 / SCF (*.ab1 *.scf);;FASTA / GenBank (*.fasta *.fa *.gb *.gbk);;FASTQ (*.fastq *.fq);;Text (*.txt);;All Files (*)");
  if (files.isEmpty()) return;
  loadFiles(files);
}

bool MainWindow::loadFiles(const QStringList &files) {
  int added = 0;
  QStringList failed;

  auto addRecords = [&](const QString &file, const std::vector<SequenceRecord> &recs) {
    if (recs.empty()) { failed << QFileInfo(file).fileName(); return; }
    for (const auto &r : recs) {
      QString id = (recs.size() > 1) ? file + " [" + r.name + "]" : file;
      if (contigWidget->addAb1Track(id, r.data)) ++added;
    }
    addRecent(file);
  };

  for (const QString &file : files) {
    auto has = [&](const char *ext) { return file.endsWith(ext, Qt::CaseInsensitive); };
    if (has(".ab1") || has(".scf")) {
      Ab1Data d = Ab1Parser::parse(file);
      if (d.isValid) { if (contigWidget->addAb1Track(file, d)) { ++added; addRecent(file); } }
      else failed << QFileInfo(file).fileName() + (d.errorMessage.isEmpty() ? QString() : " (" + d.errorMessage + ")");
    } else if (has(".fasta") || has(".fa") || has(".gb") || has(".gbk")) {
      addRecords(file, SequenceFileParser::parseFastaOrGbRecords(file));
    } else if (has(".fastq") || has(".fq")) {
      addRecords(file, SequenceFileParser::parseFastq(file));
    } else if (has(".txt")) {
      addRecords(file, SequenceFileParser::parseTxt(file));
    } else {
      failed << QFileInfo(file).fileName() + " (unsupported format)";
    }
  }

  if (!failed.isEmpty()) reportError("Open Files", "Could not load:\n" + failed.join("\n"));
  if (added > 0) contigWidget->runAlignment();
  updateWindowTitle();
  return failed.isEmpty();
}

bool MainWindow::openProjectPath(const QString &path) {
  if (!contigWidget->loadProject(path)) {
    reportError("Open Project", "The project could not be loaded. The file may be unreadable, corrupted, or not a ChromaZ project:\n" + path);
    return false;
  }
  return true;   // currentProjectPath и список недавних обновит сигнал projectLoaded
}

void MainWindow::openProject() {
  if (!maybeSave()) return;
  const QString path = QFileDialog::getOpenFileName(
      this, "Open ChromaZ Project", QString(), "ChromaZ Project (*.ChromaZ)");
  if (!path.isEmpty()) openProjectPath(path);
}

bool MainWindow::doSaveProject(const QString &path) {
  if (!contigWidget->saveProject(path)) {
    reportError("Save Project", "The project could not be saved to:\n" + path);
    return false;
  }
  currentProjectPath = path;
  contigWidget->setModified(false);
  addRecent(path);
  updateWindowTitle();
  return true;
}

void MainWindow::saveProject() {
  if (currentProjectPath.isEmpty()) { saveProjectAs(); return; }
  doSaveProject(currentProjectPath);
}

void MainWindow::saveProjectAs() {
  const QString suggested = currentProjectPath.isEmpty() ? QString("project.ChromaZ") : currentProjectPath;
  QString path = QFileDialog::getSaveFileName(this, "Save ChromaZ Project", suggested, "ChromaZ Project (*.ChromaZ)");
  if (path.isEmpty()) return;
  if (!path.endsWith(".ChromaZ", Qt::CaseInsensitive)) path += ".ChromaZ";
  doSaveProject(path);
}

bool MainWindow::maybeSave() {
  if (batchMode || !contigWidget->isModified() || !contigWidget->hasTracks()) return true;
  const auto r = QMessageBox::question(this, "Unsaved changes",
      "The workspace has unsaved changes. Save the project before continuing?",
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
  if (r == QMessageBox::Cancel) return false;
  if (r == QMessageBox::Save) { saveProject(); return !contigWidget->isModified(); }
  return true;
}

void MainWindow::updateWindowTitle() {
  QString t = "ChromaZ — Sanger Alignment & Contig Editor";
  if (!currentProjectPath.isEmpty()) t = QFileInfo(currentProjectPath).fileName() + " — ChromaZ";
  if (contigWidget && contigWidget->isModified() && contigWidget->hasTracks()) t = "* " + t;
  setWindowTitle(t);
}

// ---- недавние файлы ----
void MainWindow::addRecent(const QString &path) {
  if (settingsLocked || path.isEmpty()) return;
  QSettings settings("ChromaZ", "ChromaZ");
  QStringList list = settings.value("recentFiles").toStringList();
  list.removeAll(path);
  list.prepend(path);
  while (list.size() > 10) list.removeLast();
  settings.setValue("recentFiles", list);
}

void MainWindow::updateRecentMenu() {
  menuRecent->clear();
  QSettings settings("ChromaZ", "ChromaZ");
  const QStringList list = settings.value("recentFiles").toStringList();
  if (list.isEmpty()) {
    QAction *empty = menuRecent->addAction("(empty)");
    empty->setEnabled(false);
    return;
  }
  for (const QString &path : list) {
    QAction *a = menuRecent->addAction(QFileInfo(path).fileName() + "   —   " + QDir::toNativeSeparators(path));
    connect(a, &QAction::triggered, this, [this, path]() { openRecent(path); });
  }
  menuRecent->addSeparator();
  connect(menuRecent->addAction("Clear list"), &QAction::triggered, this, &MainWindow::clearRecent);
}

void MainWindow::clearRecent() {
  QSettings settings("ChromaZ", "ChromaZ");
  settings.remove("recentFiles");
}

void MainWindow::openRecent(const QString &path) {
  if (!QFileInfo::exists(path)) {
    QSettings settings("ChromaZ", "ChromaZ");
    QStringList list = settings.value("recentFiles").toStringList();
    list.removeAll(path);
    settings.setValue("recentFiles", list);
    reportError("Open Recent", "The file no longer exists:\n" + path);
    return;
  }
  if (path.endsWith(".ChromaZ", Qt::CaseInsensitive)) {
    if (maybeSave()) openProjectPath(path);
  } else {
    loadFiles(QStringList() << path);
  }
}

// ---- автосохранение ----
QString MainWindow::autosavePath() const {
  QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (dir.isEmpty()) dir = QDir::tempPath();
  QDir().mkpath(dir);
  return dir + "/autosave.ChromaZ";
}

void MainWindow::maybeRestoreAutosave() {
  const QFileInfo fi(autosavePath());
  if (!fi.exists() || fi.size() == 0) return;
  const auto r = QMessageBox::question(this, "Restore session",
      QString("An autosaved session from %1 was found (ChromaZ may have closed unexpectedly).\nRestore it?")
          .arg(QLocale().toString(fi.lastModified(), QLocale::ShortFormat)),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
  if (r == QMessageBox::Yes) {
    if (contigWidget->loadProject(fi.absoluteFilePath())) {
      currentProjectPath.clear();
      contigWidget->setModified(true);   // восстановленная сессия не сохранена в обычный проект
      updateWindowTitle();
    } else {
      reportError("Restore session", "The autosaved session could not be loaded.");
      QFile::remove(fi.absoluteFilePath());
    }
  } else {
    QFile::remove(fi.absoluteFilePath());
  }
}

// ---- панель покрытия ----
void MainWindow::createCoverageDock() {
  coverageDock = new QDockWidget("Coverage and Mini-map", this);
  coverageDock->setObjectName("CoverageDock");
  QWidget *box = new QWidget(coverageDock);
  QVBoxLayout *lay = new QVBoxLayout(box);
  lay->setContentsMargins(4, 2, 4, 2);
  lay->setSpacing(2);
  miniMap = new MiniMapWidget(contigWidget, scrollArea, box);
  statsLabel = new QLabel("No alignment", box);
  lay->addWidget(miniMap, 1);
  lay->addWidget(statsLabel);
  coverageDock->setWidget(box);
  addDockWidget(Qt::BottomDockWidgetArea, coverageDock);
  coverageDock->toggleViewAction()->setText("🗺 Coverage and Mini-map");

  connect(contigWidget, &ContigWidget::statsChanged, this, [this]() {
    const ColumnStats &s = contigWidget->columnStats();
    if (s.totalCols == 0 || s.maxDepth == 0) { statsLabel->setText("No alignment"); return; }
    statsLabel->setText(QString("Columns: %1   |   Covered: %2 (%3%)   |   Mean depth: %4   |   Max depth: %5   |   Depth ≥ 2: %6%   |   Low-quality columns (Q<20): %7")
        .arg(s.totalCols).arg(s.coveredCols).arg(100.0 * s.coveredCols / s.totalCols, 0, 'f', 1)
        .arg(s.meanDepth, 0, 'f', 1).arg(s.maxDepth).arg(100.0 * s.multiCols / s.totalCols, 0, 'f', 1).arg(s.lowQualCols));
  });
}

// ---- командная строка ----
void MainWindow::showCliHelpDialog() {
  QDialog dlg(this);
  dlg.setWindowTitle("Command-line options");
  dlg.resize(860, 640);
  QVBoxLayout *l = new QVBoxLayout(&dlg);
  QPlainTextEdit *edit = new QPlainTextEdit(Cli::helpText());
  edit->setReadOnly(true);
  edit->setFont(QFont("Monospace", 9));
  edit->setLineWrapMode(QPlainTextEdit::NoWrap);
  l->addWidget(edit);
  QPushButton *btn = new QPushButton("Close");
  connect(btn, &QPushButton::clicked, &dlg, &QDialog::accept);
  l->addWidget(btn, 0, Qt::AlignRight);
  dlg.exec();
}

void MainWindow::runNextCliStep() {
  while (!cliSteps.empty()) {
    if (contigWidget->isAligning()) return;       // продолжим по окончании выравнивания
    auto step = cliSteps.front();
    cliSteps.erase(cliSteps.begin());
    step();
  }
}

void MainWindow::applyCommandLine(const QCommandLineParser &p) {
  const QStringList positional = p.positionalArguments();
  bool any = !positional.isEmpty();
  for (const QString &name : p.optionNames()) if (name != "help" && name != "h" && name != "version" && name != "v") any = true;
  if (!any) return;

  settingsLocked = true;     // запуск с параметрами не должен менять сохранённые настройки
  batchMode = p.isSet("quit");
  loadingSettings = true;

  if (p.isSet("algorithm")) {
    int a = Cli::parseAlgorithm(p.value("algorithm"));
    if (a < 0) reportError("Command line", "Unknown algorithm: " + p.value("algorithm")); else algoCombo->setCurrentIndex(a);
  }
  if (p.isSet("min-identity")) {
    bool ok = false; double v = p.value("min-identity").toDouble(&ok);
    if (!ok || v < 0 || v > 100) reportError("Command line", "Invalid --min-identity value"); else identitySpin->setValue(v);
  }
  if (p.isSet("table")) {
    int t = Cli::parseTable(p.value("table"));
    if (t < 0) reportError("Command line", "Unknown genetic code: " + p.value("table")); else tableCombo->setCurrentIndex(t);
  }
  if (p.isSet("no-auto-rc")) actToggleAutoRc->setChecked(false);
  if (p.isSet("iupac")) actToggleIupac->setChecked(true);
  if (p.isSet("refine")) { bool ok = false; int v = p.value("refine").toInt(&ok); if (ok) refineSpin->setValue(v); else reportError("Command line", "Invalid --refine value"); }
  if (p.isSet("threads")) { bool ok = false; int v = p.value("threads").toInt(&ok); if (ok) threadsSpin->setValue(v); else reportError("Command line", "Invalid --threads value"); }
  if (p.isSet("no-orf-a")) actToggleOrfA->setChecked(false);
  if (p.isSet("no-orf-b")) actToggleOrfB->setChecked(false);
  if (p.isSet("restriction")) actToggleRestr->setChecked(true);
  loadingSettings = false;

  // Шаги выполняются по очереди; каждый, запускающий выравнивание, дожидается его окончания.
  QString projectPath = p.value("project");
  QStringList seqFiles;
  for (const QString &f : positional) {
    if (f.endsWith(".ChromaZ", Qt::CaseInsensitive)) { if (projectPath.isEmpty()) projectPath = f; }
    else seqFiles << f;
  }

  if (!projectPath.isEmpty()) {
    cliSteps.push_back([this, projectPath]() { openProjectPath(projectPath); });
  } else if (p.isSet("reference")) {
    const QString refPath = p.value("reference");
    cliSteps.push_back([this, refPath]() {
      auto recs = SequenceFileParser::parseFastaOrGbRecords(refPath);
      if (recs.empty()) reportError("Command line", "Could not read the reference file: " + refPath);
      else contigWidget->setReference(recs[0].data.sequence);
    });
  }
  if (!seqFiles.isEmpty()) cliSteps.push_back([this, seqFiles]() { loadFiles(seqFiles); });

  if (p.isSet("trim-quality")) {
    bool ok = false; int q = p.value("trim-quality").toInt(&ok);
    if (!ok || q < 1 || q > 60) reportError("Command line", "Invalid --trim-quality value");
    else cliSteps.push_back([this, q]() { contigWidget->trimLowQualityEnds(q); });
  }
  if (p.isSet("trim-vectors")) cliSteps.push_back([this]() { contigWidget->trimVectors(); });

  auto checkOutput = [this](const QString &path, const char *what) {
    if (QFileInfo(path).exists()) std::printf("ChromaZ: wrote %s: %s\n", what, path.toLocal8Bit().constData());
    else reportError("Export", QString("Could not write ") + what + ": " + path);
  };
  if (p.isSet("export-fasta")) { const QString f = p.value("export-fasta"); cliSteps.push_back([this, f, checkOutput]() { contigWidget->exportConsensusFasta(f); checkOutput(f, "FASTA"); }); }
  if (p.isSet("export-gbk"))   { const QString f = p.value("export-gbk");   cliSteps.push_back([this, f, checkOutput]() { contigWidget->exportConsensusGenBank(f); checkOutput(f, "GenBank"); }); }
  if (p.isSet("export-png"))   { const QString f = p.value("export-png");   cliSteps.push_back([this, f, checkOutput]() { contigWidget->exportViewImage(f); checkOutput(f, "PNG"); }); }
  if (p.isSet("export-pdf")) {
    const QString f = p.value("export-pdf");
    int bases = p.isSet("pdf-bases") ? p.value("pdf-bases").toInt() : 60;
    cliSteps.push_back([this, f, bases, checkOutput]() { contigWidget->exportPdfReport(f, bases); checkOutput(f, "PDF"); });
  }
  if (p.isSet("save-project")) {
    const QString f = p.value("save-project");
    cliSteps.push_back([this, f]() { if (contigWidget->saveProject(f)) std::printf("ChromaZ: wrote project: %s\n", f.toLocal8Bit().constData()); else reportError("Save Project", "Could not save: " + f); });
  }
  if (batchMode) cliSteps.push_back([this]() { qApp->exit(cliFailed ? 1 : 0); });

  QTimer::singleShot(0, this, [this]() { runNextCliStep(); });
}

void MainWindow::findSequenceDialog() {
  bool ok;
  QString q = QInputDialog::getText(this, "Find Sequence", "Nucleotide query string:", QLineEdit::Normal, "", &ok);
  if (ok && !q.isEmpty()) {
    if (searchEdit) searchEdit->setText(q);
    contigWidget->findSequence(q, true);
  }
}