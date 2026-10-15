#include "mainwindow.h"
#include <QMenuBar>
#include <QToolBar>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QApplication>
#include <QHBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle("ChromaZ — Sanger Alignment & Contig Editor");
  resize(1280, 800);

  contigWidget = new ContigWidget(this);
  scrollArea = new QScrollArea(this);
  scrollArea->setWidget(contigWidget);
  scrollArea->setWidgetResizable(false);
  setCentralWidget(scrollArea);

  createActions();
  createMenus();
  createToolBars();

  connect(contigWidget, &ContigWidget::restrictionSitesToggled, actToggleRestr, &QAction::setChecked);
  connect(contigWidget, &ContigWidget::speechRateChanged, this, [this](double rate) {
    if (speedSlider) {
      int val = std::round((rate + 1.0) * 50.0);
      if (speedSlider->value() != val) speedSlider->setValue(val);
    }
  });
}

void MainWindow::createActions() {
  actOpen = new QAction("📂 Open Sequencing Files...", this);
  actOpen->setShortcut(QKeySequence::Open);
  connect(actOpen, &QAction::triggered, this, &MainWindow::openFiles);

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

  actTrimQuality = new QAction("✂ Trim Low Quality (Phred)...", this);
  connect(actTrimQuality, &QAction::triggered, this, [this]() {
    bool ok;
    int q = QInputDialog::getInt(this, "Quality Trimming", "Minimum Phred Quality score:", 20, 5, 50, 1, &ok);
    if (ok) contigWidget->trimLowQualityEnds(q);
  });

  actTrimVectors = new QAction("✂ Trim Vectors (UniVec)", this);
  connect(actTrimVectors, &QAction::triggered, contigWidget, &ContigWidget::trimVectors);

  actZoomInX = new QAction("🔍+ Zoom Bases (X)", this);
  connect(actZoomInX, &QAction::triggered, contigWidget, &ContigWidget::zoomInX);

  actZoomOutX = new QAction("🔍- Zoom Bases (X)", this);
  connect(actZoomOutX, &QAction::triggered, contigWidget, &ContigWidget::zoomOutX);

  actZoomInY = new QAction("📈+ Zoom Peaks (Y)", this);
  connect(actZoomInY, &QAction::triggered, contigWidget, &ContigWidget::zoomInY);

  actZoomOutY = new QAction("📉- Zoom Peaks (Y)", this);
  connect(actZoomOutY, &QAction::triggered, contigWidget, &ContigWidget::zoomOutY);

  actResetZoom = new QAction("🔄 Reset Zoom", this);
  connect(actResetZoom, &QAction::triggered, contigWidget, &ContigWidget::resetZoom);

  actToggleOrfA = new QAction("ORF A (Reference)", this);
  actToggleOrfA->setCheckable(true);
  actToggleOrfA->setChecked(true);
  connect(actToggleOrfA, &QAction::toggled, contigWidget, &ContigWidget::setShowOrfA);

  actToggleOrfB = new QAction("ORF B (Consensus)", this);
  actToggleOrfB->setCheckable(true);
  actToggleOrfB->setChecked(true);
  connect(actToggleOrfB, &QAction::toggled, contigWidget, &ContigWidget::setShowOrfB);

  actToggleRestr = new QAction("👁 Restriction Sites", this);
  actToggleRestr->setCheckable(true);
  actToggleRestr->setChecked(false);
  connect(actToggleRestr, &QAction::toggled, contigWidget, &ContigWidget::toggleRestrictionSites);

  actBlast = new QAction("🌐 NCBI BLASTn", this);
  connect(actBlast, &QAction::triggered, contigWidget, &ContigWidget::openInNcbiBlast);

  actProps = new QAction("📊 Properties & Graphs", this);
  connect(actProps, &QAction::triggered, contigWidget, &ContigWidget::showSequenceProperties);

  actTm = new QAction("🔥 PCR Melting Temp (Tm)", this);
  connect(actTm, &QAction::triggered, contigWidget, &ContigWidget::showMeltingTemperature);

  actNextSnp = new QAction("▶ Next SNP", this);
  connect(actNextSnp, &QAction::triggered, contigWidget, &ContigWidget::jumpNextSNP);

  actPrevSnp = new QAction("◀ Prev SNP", this);
  connect(actPrevSnp, &QAction::triggered, contigWidget, &ContigWidget::jumpPrevSNP);

  actReadAloud = new QAction("🔊 Read Aloud", this);
  connect(actReadAloud, &QAction::triggered, contigWidget, &ContigWidget::readSelectionAloud);

  actStopRead = new QAction("🔇 Stop", this);
  connect(actStopRead, &QAction::triggered, contigWidget, &ContigWidget::stopReading);
}

void MainWindow::createMenus() {
  QMenu *fileMenu = menuBar()->addMenu("&File");
  fileMenu->addAction(actOpen);
  fileMenu->addAction(actOpenProj);
  fileMenu->addAction(actSaveProj);
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
  editMenu->addAction(actTrimQuality);
  editMenu->addAction(actTrimVectors);

  QMenu *viewMenu = menuBar()->addMenu("&View");
  viewMenu->addAction(actZoomInX);
  viewMenu->addAction(actZoomOutX);
  viewMenu->addAction(actZoomInY);
  viewMenu->addAction(actZoomOutY);
  viewMenu->addAction(actResetZoom);
  viewMenu->addSeparator();
  viewMenu->addAction(actToggleOrfA);
  viewMenu->addAction(actToggleOrfB);
  viewMenu->addAction(actToggleRestr);

  QMenu *analysisMenu = menuBar()->addMenu("&Analysis");
  analysisMenu->addAction(actBlast);
  analysisMenu->addAction(actProps);
  analysisMenu->addAction(actTm);
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
      {"0.5x (Very Slow)", -0.75},
      {"0.75x (Slow)", -0.35},
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
      });
  }
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
      });
  }
}

void MainWindow::createToolBars() {
  QToolBar *mainBar = addToolBar("Main Operations");
  mainBar->addAction(actOpen);
  mainBar->addAction(actSaveProj);
  mainBar->addAction(actPrintExport);
  mainBar->addSeparator();
  mainBar->addAction(actUndo);
  mainBar->addAction(actRedo);
  mainBar->addSeparator();
  mainBar->addAction(actToggleRestr);
  mainBar->addAction(actTrimVectors);
  mainBar->addSeparator();
  mainBar->addAction(actBlast);
  mainBar->addAction(actProps);
  mainBar->addAction(actTm);
  mainBar->addSeparator();
  mainBar->addAction(actPrevSnp);
  mainBar->addAction(actNextSnp);

  QToolBar *speechBar = addToolBar("Text-to-Speech Controls");
  speechBar->addAction(actReadAloud);
  speechBar->addAction(actStopRead);

  voiceCombo = new QComboBox(this);
  voiceCombo->setMinimumWidth(180);
  speechBar->addWidget(new QLabel(" 🎙 Voice: ", this));
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
    if (idx < voices.size()) contigWidget->setVoice(voices[idx]);
  });

  speechBar->addWidget(new QLabel("  ⏱ Speed: ", this));
  speedSlider = new QSlider(Qt::Horizontal, this);
  speedSlider->setRange(0, 100);
  speedSlider->setValue(50);
  speedSlider->setMaximumWidth(120);
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

void MainWindow::openFiles() {
  QStringList files = QFileDialog::getOpenFileNames(
      this, "Open Sequencing / Alignment Files", "",
      "All Supported Files (*.ab1 *.scf *.fasta *.fa *.gb *.gbk *.fastq *.fq *.txt);;AB1 Files (*.ab1);;FASTA (*.fasta *.fa);;All Files (*)");
  for (const QString &file : files) {
    if (file.endsWith(".ab1", Qt::CaseInsensitive) || file.endsWith(".scf", Qt::CaseInsensitive)) {
      Ab1Data d = Ab1Parser::parse(file);
      if (d.isValid) contigWidget->addAb1Track(file, d);
    } else if (file.endsWith(".fasta", Qt::CaseInsensitive) || file.endsWith(".fa", Qt::CaseInsensitive) ||
               file.endsWith(".gb", Qt::CaseInsensitive) || file.endsWith(".gbk", Qt::CaseInsensitive)) {
      auto recs = SequenceFileParser::parseFastaOrGbRecords(file);
      for (const auto &r : recs) contigWidget->addAb1Track(file + " [" + r.name + "]", r.data);
    }
  }
  contigWidget->runAlignment();
}

void MainWindow::openProject() {
  QString p = QFileDialog::getOpenFileName(this, "Open ChromaZ Project", "", "ChromaZ Project (*.ChromaZ)");
  if (!p.isEmpty()) contigWidget->loadProject(p);
}

void MainWindow::saveProject() {
  QString p = QFileDialog::getSaveFileName(this, "Save ChromaZ Project", "project.ChromaZ", "ChromaZ Project (*.ChromaZ)");
  if (!p.isEmpty()) contigWidget->saveProject(p);
}

void MainWindow::findSequenceDialog() {
  bool ok;
  QString q = QInputDialog::getText(this, "Find Sequence", "Nucleotide query string:", QLineEdit::Normal, "", &ok);
  if (ok && !q.isEmpty()) contigWidget->findSequence(q, true);
}\n