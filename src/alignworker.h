#ifndef ALIGNWORKER_H
#define ALIGNWORKER_H

#include <atomic>
#include <QThread>
#include <QString>
#include "alignjob.h"

// Фоновый поток выравнивания: GUI остаётся отзывчивым, прогресс приходит сигналом.
class AlignWorker : public QThread {
  Q_OBJECT
public:
  AlignWorker(AlignJobInput in, QObject *parent = nullptr) : QThread(parent), input(std::move(in)) {}
  void cancel() { cancelFlag.store(true); }

  AlignJobOutput result;                 // читать только после finished()

signals:
  void progress(int done, int total, const QString &stage);

protected:
  void run() override {
    result = runAlignJob(input, cancelFlag, [this](int d, int t, const QString &s) { emit progress(d, t, s); });
  }

private:
  AlignJobInput input;
  std::atomic<bool> cancelFlag{false};
};

#endif // ALIGNWORKER_H
