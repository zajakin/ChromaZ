#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QScrollArea>
#include <QComboBox>
#include <QLineEdit>
#include <QAction>
#include "contigwidget.h"

class MainWindow : public QMainWindow {
  Q_OBJECT
  
public:
  explicit MainWindow(QWidget *parent = nullptr);
  
protected:
  void resizeEvent(QResizeEvent *event) override;
  
  private slots:
    void openProject();
  void saveProject();
  void openAb1Files();
  void openFastaFile();
  
private:
  ContigWidget *contigView = nullptr;
  QScrollArea *scrollArea = nullptr;
  QComboBox *algoCombo = nullptr;
  QLineEdit *searchEdit = nullptr;
  QAction *iupacAction = nullptr;
  QAction *autoRCAct = nullptr;
  QAction *orfAAction = nullptr;
  QAction *orfBAction = nullptr;
  QString currentProjectPath;
};

#endif // MAINWINDOW_H