#ifndef CHROMATOGRAMWIDGET_H
#define CHROMATOGRAMWIDGET_H

#include <QWidget>
#include "ab1parser.h"

class ChromatogramWidget : public QWidget {
  Q_OBJECT
  
public:
  explicit ChromatogramWidget(QWidget *parent = nullptr);
  void loadDummyData();
  void setAb1Data(const Ab1Data &data); // Загрузка реальных данных
  
  public slots:
    void zoomInX();
  void zoomOutX();
  void zoomInY();
  void zoomOutY();
  void resetZoom();
  
protected:
  void paintEvent(QPaintEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  
private:
  Ab1Data currentData;
  double scaleX = 1.0;
  double scaleY = 0.1; // Уменьшим дефолтный масштаб Y для реальных значений AB1 (~1000-4000)
};

#endif // CHROMATOGRAMWIDGET_H