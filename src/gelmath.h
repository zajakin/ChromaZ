#ifndef GELMATH_H
#define GELMATH_H

#include <algorithm>
#include <vector>

// Размеры фрагментов после рестрикции. cuts — позиции разрезов (число оснований слева от разреза).
// Для линейной молекулы учитываются разрезы 0 < c < length; для кольцевой — c в [0, length).
// Результат отсортирован по убыванию.
inline std::vector<int> fragmentSizes(int length, std::vector<int> cuts, bool circular) {
  std::vector<int> sizes;
  if (length <= 0) return sizes;
  if (circular) {
    for (int &c : cuts) { c %= length; if (c < 0) c += length; }
  } else {
    cuts.erase(std::remove_if(cuts.begin(), cuts.end(), [&](int c) { return c <= 0 || c >= length; }), cuts.end());
  }
  std::sort(cuts.begin(), cuts.end());
  cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
  if (cuts.empty()) { sizes.push_back(length); return sizes; }
  if (circular) {
    if (cuts.size() == 1) { sizes.push_back(length); return sizes; }
    for (size_t i = 0; i + 1 < cuts.size(); ++i) sizes.push_back(cuts[i + 1] - cuts[i]);
    sizes.push_back(length - cuts.back() + cuts.front());
  } else {
    int prev = 0;
    for (int c : cuts) { sizes.push_back(c - prev); prev = c; }
    sizes.push_back(length - prev);
  }
  std::sort(sizes.begin(), sizes.end(), std::greater<int>());
  return sizes;
}

#endif // GELMATH_H
