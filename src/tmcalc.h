#ifndef TMCALC_H
#define TMCALC_H

#include <string>

// Оценки температуры плавления ДНК-олигонуклеотидов. Без зависимостей от Qt.
namespace TmCalc {

// Правило Уоллеса: 2·(A+T) + 4·(G+C), °C. Применимо к коротким (< 14 нт) олигонуклеотидам.
double wallace(const std::string &seq);

// Базовая GC-формула: 64.9 + 41·(G+C − 16.4)/N, °C. Для олигонуклеотидов >= 14 нт.
double gcFormula(const std::string &seq);

struct NNResult {
  bool valid = false;
  std::string reason;          // причина, если valid == false
  double tm = 0.0;             // °C
  double deltaH = 0.0;         // ккал/моль
  double deltaS = 0.0;         // кал/(К·моль), с поправкой на соль
  double deltaG37 = 0.0;       // ккал/моль при 37 °C
  double naEquivalentMM = 0.0; // эквивалентная концентрация одновалентных катионов, мМ
  bool selfComplementary = false;
};

// Метод ближайших соседей, унифицированные параметры SantaLucia (1998).
// naMM — Na+/K+ (мМ), mgMM — Mg2+ (мМ, необязательно), primerNM — концентрация праймера (нМ).
NNResult nearestNeighbor(const std::string &seq, double naMM, double mgMM, double primerNM);

} // namespace TmCalc

#endif // TMCALC_H
