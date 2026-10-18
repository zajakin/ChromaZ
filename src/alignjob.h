#ifndef ALIGNJOB_H
#define ALIGNJOB_H

#include <atomic>
#include <functional>
#include <vector>
#include <QString>
#include "alignmentengine.h"

// Вычислительное ядро выравнивания ридов. Не зависит от GUI и может выполняться в любом потоке.

struct AlignJobTrackIn {
  QString rawSeq;                 // последовательность рида; ' ' — удалённые пользователем основания
  bool fixedOrientation = false;  // ориентация задана вручную / Auto RC выключен
  bool fixedRC = false;           // при fixedOrientation: true — использовать обратный комплемент
};

struct AlignJobInput {
  std::vector<AlignJobTrackIn> tracks;
  QString reference;              // референс без гэпов; пусто — автоматический (самый длинный рид)
  AlignmentAlgorithm algo = AlignmentAlgorithm::OverlapCAP3;
  bool autoRC = true;
  int refineIterations = 0;       // число итераций уточнения (только для автоматического референса)
  int threads = 0;                // 0 — по числу ядер
  double minIdentity = 45.0;      // риды ниже порога не участвуют в уточнении консенсуса
};

struct AlignJobTrackOut {
  bool valid = false;             // false — у рида нет последовательности
  bool isRC = false;
  double identity = 0.0;
  int score = 0;
  QString alignedRef;             // попарное выравнивание с референсом (без master-колонок)
  QString alignedRead;            // удалённые основания возвращены как ' '
};

struct AlignJobOutput {
  bool cancelled = false;
  QString reference;              // итоговый референс без гэпов
  bool autoReference = false;
  int refineRounds = 0;
  std::vector<AlignJobTrackOut> tracks;
};

using AlignProgressFn = std::function<void(int done, int total, const QString &stage)>;

AlignJobOutput runAlignJob(const AlignJobInput &input, const std::atomic<bool> &cancel,
                           const AlignProgressFn &progress = {});

// Общий "master"-референс: референс плюс все вставки ридов; masterReads[k] — строка рида k
// той же длины (для невалидных ридов — пустая).
struct MasterAlignment {
  QString masterRef;
  std::vector<QString> masterReads;
};
MasterAlignment buildMaster(const QString &ungappedRef, const std::vector<AlignJobTrackOut> &tracks);

// Консенсус (без гэпов) по master-выравниванию; голосуют только риды с voter[k] == true.
QString consensusFromMaster(const MasterAlignment &master, const std::vector<bool> &voter);

// Возвращает в выровненный рид удалённые основания (' '), сохраняя порядок символов исходного рида.
// Пустой слот переиспользует соседний гэп, иначе добавляется новая колонка (' ' в риде, '-' в референсе).
void reinsertDeletedBases(QString &alignedRef, QString &alignedRead, const QString &orientedRaw);

#endif // ALIGNJOB_H
