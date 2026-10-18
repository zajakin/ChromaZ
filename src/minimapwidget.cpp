#include "minimapwidget.h"
#include "contigwidget.h"
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolTip>
#include <algorithm>

MiniMapWidget::MiniMapWidget(ContigWidget *c, QScrollArea *a, QWidget *parent) : QWidget(parent), contig(c), area(a) {
  setMouseTracking(true);
  setAutoFillBackground(true);
  QPalette p = palette();
  p.setColor(QPalette::Window, Qt::white);
  setPalette(p);
  connect(contig, &ContigWidget::statsChanged, this, [this]() { update(); });
  if (area && area->horizontalScrollBar()) {
    connect(area->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this]() { update(); });
    connect(area->horizontalScrollBar(), &QScrollBar::rangeChanged, this, [this]() { update(); });
  }
}

int MiniMapWidget::columnAt(int x) const {
  const int cols = (int)contig->columnStats().depth.size();
  if (cols <= 0 || width() <= 0) return -1;
  return std::clamp((int)((double)x / width() * cols), 0, cols - 1);
}

void MiniMapWidget::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(rect(), Qt::white);
  const ColumnStats &st = contig->columnStats();
  const int cols = (int)st.depth.size();
  const int W = width(), H = height();
  const int qualH = 10, snpH = 8;
  const int chartTop = 4, chartBottom = H - qualH - snpH - 4;
  if (cols <= 0 || st.maxDepth <= 0) {
    p.setPen(Qt::gray);
    p.drawText(rect(), Qt::AlignCenter, "No alignment");
    return;
  }

  // глубина покрытия
  const double colW = (double)W / cols;
  p.setPen(Qt::NoPen);
  for (int x = 0; x < W; ++x) {
    int c0 = std::min(cols - 1, (int)(x / colW)), c1 = std::min(cols, std::max(c0 + 1, (int)((x + 1) / colW)));
    int d = 0;
    for (int c = c0; c < c1; ++c) d = std::max(d, st.depth[c]);
    if (d <= 0) continue;
    int h = (int)((double)d / st.maxDepth * (chartBottom - chartTop));
    p.fillRect(x, chartBottom - h, 1, h, d >= 2 ? QColor(70, 130, 200) : QColor(235, 170, 80));
  }
  p.setPen(QColor(200, 200, 200));
  p.drawLine(0, chartBottom, W, chartBottom);

  // качество: зелёный Q>=30, жёлтый >=20, красный <20, серый — нет данных/нет покрытия
  for (int x = 0; x < W; ++x) {
    int c0 = std::min(cols - 1, (int)(x / colW)), c1 = std::min(cols, std::max(c0 + 1, (int)((x + 1) / colW)));
    float worst = 1000.0f; bool any = false;
    for (int c = c0; c < c1; ++c) if (st.depth[c] > 0 && st.meanQ[c] >= 0) { worst = std::min(worst, st.meanQ[c]); any = true; }
    QColor col = !any ? QColor(225, 225, 225) : (worst >= 30 ? QColor(80, 180, 90) : (worst >= 20 ? QColor(230, 200, 60) : QColor(215, 70, 60)));
    p.fillRect(x, chartBottom + 3, 1, qualH, col);
  }

  // SNP
  p.setPen(QColor(230, 120, 0));
  for (int c : contig->snpColumnList()) {
    int x = std::min(W - 1, (int)(c * colW));
    p.drawLine(x, chartBottom + qualH + 5, x, chartBottom + qualH + 5 + snpH - 2);
  }

  // видимая область
  if (area && area->horizontalScrollBar() && contig->columnWidth() > 0) {
    double firstCol = (area->horizontalScrollBar()->value() - contig->leftMargin()) / contig->columnWidth();
    double lastCol = (area->horizontalScrollBar()->value() + area->viewport()->width() - contig->leftMargin()) / contig->columnWidth();
    firstCol = std::clamp(firstCol, 0.0, (double)cols); lastCol = std::clamp(lastCol, 0.0, (double)cols);
    QRectF vr(firstCol * colW, 1, std::max(3.0, (lastCol - firstCol) * colW), H - 2);
    p.setBrush(QColor(60, 90, 200, 40));
    p.setPen(QPen(QColor(60, 90, 200), 1.5));
    p.drawRect(vr);
  }

  p.setPen(Qt::black);
  QFont f = font(); f.setPixelSize(9); p.setFont(f);
  p.drawText(4, 12, QString("max depth %1").arg(st.maxDepth));
}

void MiniMapWidget::mousePressEvent(QMouseEvent *e) {
  if (e->button() != Qt::LeftButton) return;
  int c = columnAt((int)e->position().x());
  if (c >= 0) contig->centerOnColumn(c);
}

void MiniMapWidget::mouseMoveEvent(QMouseEvent *e) {
  int c = columnAt((int)e->position().x());
  const ColumnStats &st = contig->columnStats();
  if (c >= 0 && c < (int)st.depth.size()) {
    QString q = st.meanQ[c] >= 0 ? QString::number(st.meanQ[c], 'f', 1) : QString("n/a");
    QToolTip::showText(e->globalPosition().toPoint(), QString("Column %1\nDepth: %2\nMean Phred: %3").arg(c + 1).arg(st.depth[c]).arg(q), this);
  }
  if (e->buttons() & Qt::LeftButton) { if (c >= 0) contig->centerOnColumn(c); }
}
