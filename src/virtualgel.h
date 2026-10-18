#ifndef VIRTUALGEL_H
#define VIRTUALGEL_H

#include <QDialog>
#include <QString>
#include <QWidget>
#include <vector>

struct GelEnzyme {
  QString name;
  QString motif;
  std::vector<int> cuts;     // позиции разрезов (число оснований слева от разреза)
  bool preselect = false;
};

struct GelLane {
  QString label;
  std::vector<int> sizes;    // размеры фрагментов, п.н.
};

// Рисует гель: лестница слева, дорожки с фрагментами, логарифмическая шкала подвижности.
class GelWidget : public QWidget {
public:
  explicit GelWidget(QWidget *parent = nullptr);
  void setLanes(const std::vector<GelLane> &l);
  QSize sizeHint() const override { return QSize(560, 520); }

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  std::vector<GelLane> lanes;
};

class VirtualGelDialog : public QDialog {
public:
  VirtualGelDialog(const QString &seqTitle, int seqLength, std::vector<GelEnzyme> enzymes, QWidget *parent = nullptr);
  ~VirtualGelDialog() override;

private:
  void rebuild();

  struct Impl;
  Impl *d = nullptr;
};

#endif // VIRTUALGEL_H
