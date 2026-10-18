#ifndef MINIMAPWIDGET_H
#define MINIMAPWIDGET_H

#include <QWidget>

class ContigWidget;
class QScrollArea;

// Миникарта контига: глубина покрытия, цветовая полоса качества, SNP и видимая область.
class MiniMapWidget : public QWidget {
public:
  MiniMapWidget(ContigWidget *contig, QScrollArea *area, QWidget *parent = nullptr);
  QSize sizeHint() const override { return QSize(700, 92); }
  QSize minimumSizeHint() const override { return QSize(200, 70); }

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;

private:
  int columnAt(int x) const;
  ContigWidget *contig = nullptr;
  QScrollArea *area = nullptr;
};

#endif // MINIMAPWIDGET_H
