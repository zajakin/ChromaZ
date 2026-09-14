#include "mainwindow.h"
#include <QMenuBar>
#include <QFileDialog>
#include <QToolBar>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QScrollBar>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QAction>
#include <QKeySequence>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle("ChromaZ - Contig Alignment Viewer");
  resize(1300, 800);
  
  contigView = new ContigWidget(this);
  scrollArea = new QScrollArea(this);
  scrollArea->setStyleSheet("QScrollArea { background-color: #ffffff; }");
  scrollArea->setWidget(contigView);
  scrollArea->setWidgetResizable(true);
  
  connect(contigView, &ContigWidget::projectLoaded, this, [this](const QString &path) {
    currentProjectPath = path;
    setWindowTitle(QString("ChromaZ - %1").arg(QFileInfo(path).fileName()));
    if (iupacAction) iupacAction->setChecked(contigView->isIupacConsensusEnabled());
    if (orfAAction) orfAAction->setChecked(contigView->isOrfAEnabled());
    if (orfBAction) orfBAction->setChecked(contigView->isOrfBEnabled());
    if (algoCombo) {
      algoCombo->blockSignals(true);
      algoCombo->setCurrentIndex(static_cast<int>(contigView->getAlignmentAlgorithm()));
      algoCombo->blockSignals(false);
    }
  });
  
  QAction *openProjAct = new QAction("📁 Open Project (.ChromaZ)...", this);
  openProjAct->setShortcut(QKeySequence::Open);
  connect(openProjAct, &QAction::triggered, this, &MainWindow::openProject);
  
  QAction *saveProjAct = new QAction("💾 Save Project (.ChromaZ)", this);
  saveProjAct->setShortcut(QKeySequence::Save);
  connect(saveProjAct, &QAction::triggered, this, &MainWindow::saveProject);
  
  QAction *undoAct = new QAction("↩ Undo", this);
  undoAct->setShortcut(QKeySequence::Undo);
  connect(undoAct, &QAction::triggered, contigView, &ContigWidget::undo);
  
  QAction *redoAct = new QAction("↪ Redo", this);
  redoAct->setShortcut(QKeySequence::Redo);
  connect(redoAct, &QAction::triggered, contigView, &ContigWidget::redo);
  
  QAction *copyAct = new QAction("📋 Copy Sequence", this);
  copyAct->setShortcut(QKeySequence::Copy);
  connect(copyAct, &QAction::triggered, contigView, &ContigWidget::copySelectedToClipboard);
  
  QAction *copyImgAct = new QAction("🖼 Copy Selection as Image", this);
  connect(copyImgAct, &QAction::triggered, contigView, &ContigWidget::copySelectionAsImageToClipboard);
  
  QAction *findAct = new QAction("🔍 Find Sequence...", this);
  findAct->setShortcut(QKeySequence::Find);
  connect(findAct, &QAction::triggered, this, [this]() {
    if (searchEdit) {
      searchEdit->setFocus();
      searchEdit->selectAll();
    }
  });
  
  QAction *loadReadsAct = new QAction("📂 Load Reads (.ab1, .scf, .fasta, .fastq, .txt, .gb)...", this);
  connect(loadReadsAct, &QAction::triggered, this, &MainWindow::openAb1Files);
  
  QAction *loadRefAct = new QAction("📖 Load Reference (.fasta, .txt)...", this);
  connect(loadRefAct, &QAction::triggered, this, &MainWindow::openFastaFile);
  
  QAction *exportFastaAct = new QAction("💾 Export Consensus (.fasta)...", this);
  connect(exportFastaAct, &QAction::triggered, this, [this]() {
    QString fn = QFileDialog::getSaveFileName(this, "Export Consensus", "consensus.fasta", "FASTA (*.fasta)");
    if (!fn.isEmpty()) contigView->exportConsensusFasta(fn);
  });
  
  QAction *exportPngAct = new QAction("🖼 Export View (.png)...", this);
  connect(exportPngAct, &QAction::triggered, this, [this]() {
    QString fn = QFileDialog::getSaveFileName(this, "Export View", "alignment.png", "Images (*.png)");
    if (!fn.isEmpty()) contigView->exportViewImage(fn);
  });
  
  QAction *exportPdfAct = new QAction("📄 Export PDF...", this);
  connect(exportPdfAct, &QAction::triggered, this, [this]() {
    bool ok = false;
    int basesPerLine = QInputDialog::getInt(this, "Export PDF", "Number of bases per line:", 80, 10, 500, 1, &ok);
    if (!ok) return;
    
    QString fileName = QFileDialog::getSaveFileName(this, "Export PDF", "contig_alignment.pdf", "PDF Files (*.pdf)");
    if (!fileName.isEmpty()) contigView->exportPdf(fileName, basesPerLine);
  });
  
  QAction *alignAct = new QAction("⚡ Run Alignment", this);
  connect(alignAct, &QAction::triggered, contigView, &ContigWidget::runAlignment);
  
  iupacAction = new QAction("IUPAC Consensus", this);
  iupacAction->setCheckable(true);
  iupacAction->setChecked(false);
  connect(iupacAction, &QAction::toggled, contigView, &ContigWidget::setConsensusIupac);
  
  orfAAction = new QAction("🧬 ORF A (Ref/Contig)", this);
  orfAAction->setCheckable(true);
  orfAAction->setChecked(true);
  connect(orfAAction, &QAction::toggled, contigView, &ContigWidget::setShowOrfA);
  
  orfBAction = new QAction("🧬 ORF B (Consensus)", this);
  orfBAction->setCheckable(true);
  orfBAction->setChecked(true);
  connect(orfBAction, &QAction::toggled, contigView, &ContigWidget::setShowOrfB);
  
  autoRCAct = new QAction("🔄 Auto RC", this);
  autoRCAct->setCheckable(true);
  autoRCAct->setChecked(true);
  connect(autoRCAct, &QAction::toggled, contigView, &ContigWidget::setAutoRC);
  connect(contigView, &ContigWidget::autoRCChanged, this, [this](bool enabled) {
    if (autoRCAct) {
      autoRCAct->blockSignals(true);
      autoRCAct->setChecked(enabled);
      autoRCAct->blockSignals(false);
    }
  });
  
  QAction *trimAct = new QAction("✂ Trim Low QC", this);
  connect(trimAct, &QAction::triggered, this, [this]() { contigView->trimLowQualityEnds(20); });
  
  QAction *prevSnpAct = new QAction("◀ SNP", this);
  connect(prevSnpAct, &QAction::triggered, contigView, &ContigWidget::jumpPrevSNP);
  
  QAction *nextSnpAct = new QAction("SNP ▶", this);
  connect(nextSnpAct, &QAction::triggered, contigView, &ContigWidget::jumpNextSNP);
  
  QAction *zoomInXAct = new QAction("X +", this);
  connect(zoomInXAct, &QAction::triggered, contigView, &ContigWidget::zoomInX);
  
  QAction *zoomOutXAct = new QAction("X -", this);
  connect(zoomOutXAct, &QAction::triggered, contigView, &ContigWidget::zoomOutX);
  
  QAction *zoomInYAct = new QAction("Peak Y +", this);
  connect(zoomInYAct, &QAction::triggered, contigView, &ContigWidget::zoomInY);
  
  QAction *zoomOutYAct = new QAction("Peak Y -", this);
  connect(zoomOutYAct, &QAction::triggered, contigView, &ContigWidget::zoomOutY);
  
  QAction *rowIncAct = new QAction("Row H +", this);
  connect(rowIncAct, &QAction::triggered, contigView, &ContigWidget::increaseTrackHeight);
  
  QAction *rowDecAct = new QAction("Row H -", this);
  connect(rowDecAct, &QAction::triggered, contigView, &ContigWidget::decreaseTrackHeight);
  
  QAction *zoomResetAct = new QAction("Reset Zoom", this);
  connect(zoomResetAct, &QAction::triggered, contigView, &ContigWidget::resetZoom);
  
  QMenu *fileMenu = menuBar()->addMenu("File");
  fileMenu->addAction(openProjAct);
  fileMenu->addAction(saveProjAct);
  fileMenu->addSeparator();
  fileMenu->addAction(loadReadsAct);
  fileMenu->addAction(loadRefAct);
  fileMenu->addSeparator();
  fileMenu->addAction(exportFastaAct);
  fileMenu->addAction(exportPngAct);
  fileMenu->addAction(exportPdfAct);
  
  QMenu *editMenu = menuBar()->addMenu("Edit");
  editMenu->addAction(undoAct);
  editMenu->addAction(redoAct);
  editMenu->addSeparator();
  editMenu->addAction(findAct);
  editMenu->addAction(copyAct);
  editMenu->addAction(copyImgAct);
  
  QMenu *viewMenu = menuBar()->addMenu("View");
  viewMenu->addAction(orfAAction);
  viewMenu->addAction(orfBAction);
  viewMenu->addSeparator();
  viewMenu->addAction(prevSnpAct);
  viewMenu->addAction(nextSnpAct);
  viewMenu->addSeparator();
  viewMenu->addAction(zoomInXAct);
  viewMenu->addAction(zoomOutXAct);
  viewMenu->addAction(zoomInYAct);
  viewMenu->addAction(zoomOutYAct);
  viewMenu->addAction(rowIncAct);
  viewMenu->addAction(rowDecAct);
  viewMenu->addAction(zoomResetAct);
  
  QMenu *toolsMenu = menuBar()->addMenu("Tools");
  toolsMenu->addAction(alignAct);
  toolsMenu->addAction(autoRCAct);
  toolsMenu->addAction(iupacAction);
  toolsMenu->addAction(orfAAction);
  toolsMenu->addAction(orfBAction);
  toolsMenu->addAction(trimAct);
  
  QToolBar *fileToolBar = addToolBar("File");
  QAction *tbOpen = fileToolBar->addAction("📁 Open");
  connect(tbOpen, &QAction::triggered, openProjAct, &QAction::trigger);
  QAction *tbSave = fileToolBar->addAction("💾 Save");
  connect(tbSave, &QAction::triggered, saveProjAct, &QAction::trigger);
  
  fileToolBar->addSeparator();
  fileToolBar->addAction(undoAct);
  fileToolBar->addAction(redoAct);
  fileToolBar->addSeparator();
  
  QAction *tbLoadR = fileToolBar->addAction("📂 Load Reads");
  connect(tbLoadR, &QAction::triggered, loadReadsAct, &QAction::trigger);
  QAction *tbLoadRef = fileToolBar->addAction("📖 Load Ref");
  connect(tbLoadRef, &QAction::triggered, loadRefAct, &QAction::trigger);
  fileToolBar->addSeparator();
  
  QAction *tbPdf = fileToolBar->addAction("📄 PDF");
  connect(tbPdf, &QAction::triggered, exportPdfAct, &QAction::trigger);
  
  QToolBar *alignToolBar = addToolBar("Alignment");
  alignToolBar->addAction(alignAct);
  alignToolBar->addAction(autoRCAct);
  alignToolBar->addAction(trimAct);
  alignToolBar->addWidget(new QLabel(" Algo: ", this));
  
  algoCombo = new QComboBox(this);
  algoCombo->addItem("OLC / CAP3 (Sanger Default)", 0);
  algoCombo->addItem("Affine-Gap Block (Gotoh)", 1);
  algoCombo->addItem("Needleman-Wunsch (Global)", 2);
  algoCombo->addItem("Smith-Waterman (Local)", 3);
  algoCombo->addItem("BLAST (Seed & Extend)", 4);
  algoCombo->addItem("Banded Alignment (Fast)", 5);
  algoCombo->addItem("Wavefront Alignment (WFA)", 6);
  algoCombo->addItem("Progressive Clustal (Built-in)", 7);
  alignToolBar->addWidget(algoCombo);
  connect(algoCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), contigView, &ContigWidget::setAlignmentAlgorithm);
  
  alignToolBar->addWidget(new QLabel(" Min Id: ", this));
  QDoubleSpinBox *identitySpin = new QDoubleSpinBox(this);
  identitySpin->setRange(0.0, 100.0);
  identitySpin->setValue(45.0);
  identitySpin->setSuffix("%");
  alignToolBar->addWidget(identitySpin);
  connect(identitySpin, &QDoubleSpinBox::valueChanged, contigView, &ContigWidget::setMinIdentityThreshold);
  
  addToolBarBreak(Qt::TopToolBarArea);
  
  QToolBar *transToolBar = addToolBar("Translation & Consensus");
  transToolBar->addAction(orfAAction);
  transToolBar->addAction(orfBAction);
  transToolBar->addSeparator();
  transToolBar->addAction(iupacAction);
  transToolBar->addSeparator();
  transToolBar->addWidget(new QLabel(" Translation: ", this));
  QComboBox *transCombo = new QComboBox(this);
  transCombo->addItem("Standard (Human)", 0);
  transCombo->addItem("Vert. Mitochondrial", 1);
  transCombo->addItem("Bact. / Plant Plastid", 2);
  transToolBar->addWidget(transCombo);
  connect(transCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), contigView, &ContigWidget::setTranslationTable);
  
  QToolBar *viewTB = addToolBar("View");
  viewTB->addAction(prevSnpAct);
  viewTB->addAction(nextSnpAct);
  viewTB->addSeparator();
  viewTB->addAction(zoomInXAct);
  viewTB->addAction(zoomOutXAct);
  viewTB->addAction(zoomInYAct);
  viewTB->addAction(zoomOutYAct);
  viewTB->addAction(rowIncAct);
  viewTB->addAction(rowDecAct);
  
  QToolBar *searchToolBar = addToolBar("Search");
  searchToolBar->addWidget(new QLabel(" 🔍 ", this));
  searchEdit = new QLineEdit(this);
  searchEdit->setPlaceholderText("Search...");
  searchEdit->setMaximumWidth(130);
  searchToolBar->addWidget(searchEdit);
  
  QAction *searchNextAct = searchToolBar->addAction("▶");
  QAction *searchPrevAct = searchToolBar->addAction("◀");
  
  connect(searchEdit, &QLineEdit::returnPressed, this, [this]() { contigView->findSequence(searchEdit->text(), true); });
  connect(searchNextAct, &QAction::triggered, this, [this]() { contigView->findSequence(searchEdit->text(), true); });
  connect(searchPrevAct, &QAction::triggered, this, [this]() { contigView->findSequence(searchEdit->text(), false); });
  
  connect(scrollArea->horizontalScrollBar(), &QScrollBar::valueChanged, contigView, qOverload<>(&QWidget::update));
  setCentralWidget(scrollArea);
}

void MainWindow::saveProject() {
  if (currentProjectPath.isEmpty()) {
    currentProjectPath = QFileDialog::getSaveFileName(this, "Save Project", "project.ChromaZ", "ChromaZ Project (*.ChromaZ)");
  }
  if (!currentProjectPath.isEmpty()) {
    if (!currentProjectPath.endsWith(".ChromaZ", Qt::CaseInsensitive)) {
      currentProjectPath += ".ChromaZ";
    }
    if (contigView->saveProject(currentProjectPath)) {
      setWindowTitle(QString("ChromaZ - %1").arg(QFileInfo(currentProjectPath).fileName()));
    } else {
      QMessageBox::critical(this, "Error", "Failed to save project file.");
    }
  }
}

void MainWindow::openProject() {
  QString fileName = QFileDialog::getOpenFileName(this, "Open Project", "", "ChromaZ Project (*.ChromaZ)");
  if (!fileName.isEmpty()) {
    if (contigView->loadProject(fileName)) {
      currentProjectPath = fileName;
      setWindowTitle(QString("ChromaZ - %1").arg(QFileInfo(fileName).fileName()));
      contigView->fitToWindowHeight(scrollArea->viewport()->height());
      if (iupacAction) iupacAction->setChecked(contigView->isIupacConsensusEnabled());
      if (orfAAction) orfAAction->setChecked(contigView->isOrfAEnabled());
      if (orfBAction) orfBAction->setChecked(contigView->isOrfBEnabled());
      if (algoCombo) {
        algoCombo->blockSignals(true);
        algoCombo->setCurrentIndex(static_cast<int>(contigView->getAlignmentAlgorithm()));
        algoCombo->blockSignals(false);
      }
    } else {
      QMessageBox::critical(this, "Error", "Failed to open project file.");
    }
  }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
  if (contigView && scrollArea) {
    contigView->fitToWindowHeight(scrollArea->viewport()->height());
  }
}

void MainWindow::openAb1Files() {
  QStringList files = QFileDialog::getOpenFileNames(this, "Select Sequence Files", "", "Sequences (*.ab1 *.scf *.fasta *.fa *.fastq *.fq *.txt *.gb *.gbk);;All Files (*)");
  for (const QString &file : files) {
    if (file.endsWith(".fastq", Qt::CaseInsensitive) || file.endsWith(".fq", Qt::CaseInsensitive)) {
      auto records = SequenceFileParser::parseFastq(file);
      for (const auto &rec : records) {
        QString trackId = (records.size() > 1) ? QString("%1 [%2]").arg(QFileInfo(file).fileName()).arg(rec.name) : file;
        contigView->addAb1Track(trackId, rec.data);
      }
      continue;
    }
    
    if (file.endsWith(".txt", Qt::CaseInsensitive)) {
      auto records = SequenceFileParser::parseTxt(file);
      for (const auto &rec : records) {
        QString trackId = (records.size() > 1) ? QString("%1 [%2]").arg(QFileInfo(file).fileName()).arg(rec.name) : file;
        contigView->addAb1Track(trackId, rec.data);
      }
      continue;
    }
    
    if (contigView->isFileLoaded(file)) continue;
    
    if (file.endsWith(".ab1", Qt::CaseInsensitive) || file.endsWith(".scf", Qt::CaseInsensitive)) {
      Ab1Data data = Ab1Parser::parse(file);
      if (data.isValid) contigView->addAb1Track(file, data);
    } else {
      auto records = SequenceFileParser::parseFastaOrGbRecords(file);
      for (const auto &rec : records) {
        QString trackId = (records.size() > 1) ? QString("%1 [%2]").arg(QFileInfo(file).fileName()).arg(rec.name) : file;
        contigView->addAb1Track(trackId, rec.data);
      }
    }
  }
  contigView->fitToWindowHeight(scrollArea->viewport()->height());
}

void MainWindow::openFastaFile() {
  QString fileName = QFileDialog::getOpenFileName(this, "Open Reference Sequence", "", "Sequences (*.fasta *.fa *.gb *.gbk *.txt);;All Files (*)");
  if (fileName.isEmpty()) return;
  
  auto records = SequenceFileParser::parseFastaOrGbRecords(fileName);
  if (records.empty() && fileName.endsWith(".txt", Qt::CaseInsensitive)) {
    records = SequenceFileParser::parseTxt(fileName);
  }
  
  if (!records.empty()) {
    contigView->setReference(records[0].data.sequence);
    contigView->fitToWindowHeight(scrollArea->viewport()->height());
  }
}