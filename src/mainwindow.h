#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QScrollArea>
#include <QLineEdit>
#include <QComboBox>
#include <QAction>
#include <QString>
#include <QResizeEvent>

#include "contigwidget.h"

class MainWindow : public QMainWindow {
  Q_OBJECT
  
public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override = default;
  
protected:
  // Переопределение события изменения размера окна
  void resizeEvent(QResizeEvent *event) override;
  
  private slots:
    // Слоты, явно реализованные в mainwindow.cpp
    void openProject();
  void saveProject();
  void openAb1Files();
  void openFastaFile();
  void showAboutDialog();
  
private:
  // Указатели на виджеты и экшены, используемые в конструкторе и методах
  ContigWidget *contigView = nullptr;
  QScrollArea *scrollArea = nullptr;
  QLineEdit *searchEdit = nullptr;
  QComboBox *algoCombo = nullptr;
  
  QAction *iupacAction = nullptr;
  QAction *orfAAction = nullptr;
  QAction *orfBAction = nullptr;
  QAction *autoRCAct = nullptr;
  
  QString currentProjectPath;
};

#endif // MAINWINDOW_H