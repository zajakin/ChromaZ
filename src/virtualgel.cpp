#include "virtualgel.h"
#include "gelmath.h"
#include <QCheckBox>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {
const std::vector<int> kLadder = {10000, 8000, 6000, 5000, 4000, 3000, 2500, 2000, 1500, 1000, 750, 500, 250};
const int kMaxLanes = 12;
}

// ------------------------------- GelWidget ---------------------------------
GelWidget::GelWidget(QWidget *parent) : QWidget(parent) {
  setMinimumSize(420, 380);
  setAutoFillBackground(true);
  QPalette p = palette();
  p.setColor(QPalette::Window, Qt::white);
  setPalette(p);
}

void GelWidget::setLanes(const std::vector<GelLane> &l) { lanes = l; update(); }

void GelWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.fillRect(rect(), Qt::white);

  const int leftM = 62, topM = 70, rightM = 12, bottomM = 14;
  QRect gel(leftM, topM, width() - leftM - rightM, height() - topM - bottomM);
  p.fillRect(gel, QColor(30, 30, 34));

  int maxSize = 10000, minSize = 250;
  for (const auto &ln : lanes) for (int s : ln.sizes) { maxSize = std::max(maxSize, s); minSize = std::min(minSize, s); }
  double hi = std::log10(maxSize * 1.15), lo = std::log10(std::max(20.0, minSize * 0.8));
  auto yOf = [&](double size) {
    double t = (hi - std::log10(std::max(1.0, size))) / (hi - lo);
    return gel.top() + std::clamp(t, 0.0, 1.0) * gel.height();
  };

  const int laneCount = (int)lanes.size() + 1; // + лестница
  const double laneW = double(gel.width()) / laneCount;
  QFont small = font(); small.setPixelSize(10);
  p.setFont(small);

  // лунки
  for (int i = 0; i < laneCount; ++i) {
    QRectF well(gel.left() + i * laneW + laneW * 0.15, gel.top() + 3, laneW * 0.7, 5);
    p.fillRect(well, QColor(10, 10, 12));
  }

  // лестница
  for (int s : kLadder) {
    double y = yOf(s);
    bool bright = (s == 1000 || s == 3000 || s == 500);
    p.fillRect(QRectF(gel.left() + laneW * 0.15, y - 1.5, laneW * 0.7, 3), bright ? QColor(255, 255, 255) : QColor(190, 190, 195));
    p.setPen(Qt::black);
    p.drawText(QRectF(2, y - 7, leftM - 8, 14), Qt::AlignRight | Qt::AlignVCenter, QString::number(s));
  }
  p.setPen(Qt::black);
  p.drawText(QRectF(gel.left(), 8, laneW, 14), Qt::AlignHCenter, "Ladder");

  // дорожки
  for (size_t i = 0; i < lanes.size(); ++i) {
    const GelLane &ln = lanes[i];
    double x0 = gel.left() + (i + 1) * laneW;
    int laneMax = 1;
    for (int s : ln.sizes) laneMax = std::max(laneMax, s);
    for (int s : ln.sizes) {
      double y = yOf(s);
      int a = (int)(110 + 145.0 * std::min(1.0, (double)s / laneMax));   // масса ∝ длине
      p.fillRect(QRectF(x0 + laneW * 0.15, y - 1.5, laneW * 0.7, 3), QColor(a, a, std::min(255, a + 5)));
    }
    p.save();
    p.translate(x0 + laneW * 0.5, gel.top() - 6);
    p.rotate(-45);
    p.setPen(Qt::black);
    p.drawText(QPointF(0, 0), ln.label);
    p.restore();
  }
}

// ---------------------------- VirtualGelDialog -----------------------------
struct VirtualGelDialog::Impl {
  QString title;
  int length = 0;
  std::vector<GelEnzyme> enzymes;
  QListWidget *list = nullptr;
  QLineEdit *filter = nullptr;
  QCheckBox *circular = nullptr;
  QCheckBox *uncut = nullptr;
  GelWidget *gel = nullptr;
  QPlainTextEdit *report = nullptr;
  QLabel *warn = nullptr;
};

VirtualGelDialog::VirtualGelDialog(const QString &seqTitle, int seqLength, std::vector<GelEnzyme> enzymes, QWidget *parent)
    : QDialog(parent), d(new Impl) {
  d->title = seqTitle; d->length = seqLength; d->enzymes = std::move(enzymes);
  setWindowTitle("Virtual Gel — restriction digest");
  resize(980, 640);

  auto *root = new QHBoxLayout(this);

  auto *left = new QVBoxLayout();
  left->addWidget(new QLabel(QString("<b>%1</b> — %2 bp<br>Select enzymes (up to %3):").arg(seqTitle).arg(seqLength).arg(kMaxLanes)));
  d->filter = new QLineEdit(); d->filter->setPlaceholderText("Filter enzymes…"); d->filter->setClearButtonEnabled(true);
  left->addWidget(d->filter);
  d->list = new QListWidget();
  d->list->setMinimumWidth(230);
  for (const auto &e : d->enzymes) {
    auto *it = new QListWidgetItem(QString("%1  (%2 cut%3)").arg(e.name).arg(e.cuts.size()).arg(e.cuts.size() == 1 ? "" : "s"));
    it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
    it->setCheckState(e.preselect ? Qt::Checked : Qt::Unchecked);
    d->list->addItem(it);
  }
  left->addWidget(d->list, 1);
  d->circular = new QCheckBox("Circular molecule (plasmid)");
  d->uncut = new QCheckBox("Add undigested lane");
  left->addWidget(d->circular);
  left->addWidget(d->uncut);
  d->warn = new QLabel(); d->warn->setStyleSheet("color: #a00;"); d->warn->setWordWrap(true);
  left->addWidget(d->warn);
  root->addLayout(left);

  auto *right = new QVBoxLayout();
  d->gel = new GelWidget();
  right->addWidget(d->gel, 3);
  d->report = new QPlainTextEdit();
  d->report->setReadOnly(true);
  d->report->setFont(QFont("Monospace", 9));
  d->report->setMaximumHeight(170);
  right->addWidget(d->report, 1);
  auto *btns = new QHBoxLayout();
  auto *savePng = new QPushButton("Save gel as PNG…");
  auto *close = new QPushButton("Close");
  btns->addStretch(); btns->addWidget(savePng); btns->addWidget(close);
  right->addLayout(btns);
  root->addLayout(right, 1);

  connect(d->list, &QListWidget::itemChanged, this, [this]() { rebuild(); });
  connect(d->circular, &QCheckBox::toggled, this, [this]() { rebuild(); });
  connect(d->uncut, &QCheckBox::toggled, this, [this]() { rebuild(); });
  connect(d->filter, &QLineEdit::textChanged, this, [this](const QString &t) {
    for (int i = 0; i < d->list->count(); ++i) d->list->item(i)->setHidden(!t.isEmpty() && !d->list->item(i)->text().contains(t, Qt::CaseInsensitive));
  });
  connect(close, &QPushButton::clicked, this, &QDialog::accept);
  connect(savePng, &QPushButton::clicked, this, [this]() {
    QString path = QFileDialog::getSaveFileName(this, "Save gel image", "virtual_gel.png", "PNG (*.png)");
    if (path.isEmpty()) return;
    if (!d->gel->grab().save(path)) QMessageBox::warning(this, "Virtual Gel", "Could not save:\n" + path);
  });
  rebuild();
}

VirtualGelDialog::~VirtualGelDialog() { delete d; }

void VirtualGelDialog::rebuild() {
  std::vector<GelLane> lanes;
  QString text;
  int checked = 0;
  const bool circ = d->circular->isChecked();
  if (d->uncut->isChecked()) {
    lanes.push_back({"Uncut", {d->length}});
    text += QString("Uncut: %1 bp\n").arg(d->length);
  }
  for (int i = 0; i < d->list->count() && i < (int)d->enzymes.size(); ++i) {
    if (d->list->item(i)->checkState() != Qt::Checked) continue;
    ++checked;
    if ((int)lanes.size() >= kMaxLanes) continue;
    const GelEnzyme &e = d->enzymes[i];
    std::vector<int> sizes = fragmentSizes(d->length, e.cuts, circ);
    lanes.push_back({e.name, sizes});
    QStringList parts;
    for (int s : sizes) parts << QString::number(s);
    QStringList cutList;
    std::vector<int> cuts = e.cuts; std::sort(cuts.begin(), cuts.end());
    for (int c : cuts) cutList << QString::number(c);
    text += QString("%1 (%2 cuts at %3): %4 bp\n").arg(e.name).arg(cuts.size()).arg(cutList.join(", ")).arg(parts.join(", "));
  }
  d->warn->setText(checked > kMaxLanes ? QString("Only the first %1 selected enzymes are shown.").arg(kMaxLanes) : QString());
  d->gel->setLanes(lanes);
  d->report->setPlainText(text.isEmpty() ? "Select one or more enzymes in the list." : text);
}
