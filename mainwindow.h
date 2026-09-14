#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QScrollArea>
#include <QLineEdit>
#include <QAction>
#include <QComboBox>
#include <QResizeEvent>
#include "contigwidget.h"

class MainWindow : public QMainWindow {
  Q_OBJECT
  
public:
  MainWindow(QWidget *parent = nullptr);
  
protected:
  void resizeEvent(QResizeEvent *event) override;
  
  private slots:
    void openAb1Files();
  void openFastaFile();
  void saveProject();
  void openProject();
  
private:
  ContigWidget *contigView;
  QScrollArea *scrollArea;
  QLineEdit *searchEdit;
  QAction *iupacAction;
  QAction *autoRCAct;
  QComboBox *algoCombo;
  QString currentProjectPath;
};

#endif // MAINWINDOW_H