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
  
  // Единое действие печати и экспорта PDF
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
  editMenu->addAction