#include "chromatogramwidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <cmath>
#include <algorithm>

ChromatogramWidget::ChromatogramWidget(QWidget *parent) : QWidget(parent) {
  setBackgroundRole(QPalette::Base);
  setAutoFillBackground(true);
  setMinimumHeight(250);
  loadDummyData();
}

void ChromatogramWidget::loadDummyData() {
  currentData.traceA.clear(); currentData.traceC.clear();
  currentData.traceG.clear(); currentData.traceT.clear();
  for (int i = 0; i < 2000; ++i) {
    currentData.traceA.push_back(std::abs(sin(i * 0.1)) * 500);
    currentData.traceC.push_back(std::abs(sin((i + 15) * 0.12)) * 400);
    currentData.traceG.push_back(std::abs(sin((i + 30) * 0.08)) * 450);
    currentData.traceT.push_back(std::abs(sin((i + 45) * 0.11)) * 550);
  }
  currentData.isValid = true;
  update();
}

void ChromatogramWidget::setAb1Data(const Ab1Data &data) {
  currentData = data;
  resetZoom();
}

void ChromatogramWidget::zoomInX() { scaleX *= 1.2; update(); }
void ChromatogramWidget::zoomOutX() { scaleX = std::max(0.001, scaleX / 1.2); update(); }
void ChromatogramWidget::zoomInY() { scaleY *= 1.2; update(); }
void ChromatogramWidget::zoomOutY() { scaleY = std::max(0.0001, scaleY / 1.2); update(); }
void ChromatogramWidget::resetZoom() { scaleX = 1.0; scaleY = 0.1; update(); }

void ChromatogramWidget::paintEvent(QPaintEvent *event) {
  if (!currentData.isValid) return;
  
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  
  int h = height();
  int textMargin = 25; // Отступ сверху под буквы
  
  auto drawTrace = [&](const std::vector<int>& trace, QColor color) {
    if (trace.empty()) return;
    painter.setPen(QPen(color, 1.2));
    QPainterPath path;
    path.moveTo(0, h - trace[0] * scaleY);
    for (size_t i = 1; i < trace.size(); ++i) {
      path.lineTo(i * scaleX, h - trace[i] * scaleY);
    }
    painter.drawPath(path);
  };
  
  // Отрисовка графиков
  drawTrace(currentData.traceA, Qt::green);
  drawTrace(currentData.traceC, Qt::blue);
  drawTrace(currentData.traceG, Qt::black);
  drawTrace(currentData.traceT, Qt::red);
  
  // Отрисовка букв над пиками
  QFont font = painter.font();
  font.setBold(true);
  font.setPixelSize(12);
  painter.setFont(font);
  
  for (size_t i = 0; i < currentData.sequence.length() && i < currentData.basePositions.size(); ++i) {
    int posX = currentData.basePositions[i] * scaleX;
    QChar base = currentData.sequence[i];
    
    if (base == 'A') painter.setPen(Qt::green);
    else if (base == 'C') painter.setPen(Qt::blue);
    else if (base == 'G') painter.setPen(Qt::black);
    else if (base == 'T') painter.setPen(Qt::red);
    else painter.setPen(Qt::gray);
    
    painter.drawText(posX - 4, textMargin, QString(base));
  }
}

void ChromatogramWidget::wheelEvent(QWheelEvent *event) {
  if (event->modifiers() & Qt::ControlModifier) {
    if (event->angleDelta().y() > 0) zoomInY();
    else zoomOutY();
  } else {
    if (event->angleDelta().y() > 0) zoomInX();
    else zoomOutX();
  }
}