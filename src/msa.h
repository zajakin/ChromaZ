#ifndef MSA_H
#define MSA_H

#include <functional>
#include <string>
#include <vector>

// Прогрессивное множественное выравнивание в стиле Clustal:
//   1) попарные расстояния по общим k-мерам,
//   2) направляющее дерево UPGMA,
//   3) последовательное выравнивание профилей (полу-глобальное, концевые гэпы бесплатны).
// Чистый C++ без зависимостей от Qt.
namespace Msa {

// Возвращает выровненные строки в том же порядке и количестве, что и входные (одинаковой длины).
// Пустой результат — если операция отменена через cancelled().
std::vector<std::string> progressiveAlign(const std::vector<std::string> &seqs,
                                          const std::function<bool()> &cancelled = {},
                                          const std::function<void(int done, int total)> &progress = {});

} // namespace Msa

#endif // MSA_H
