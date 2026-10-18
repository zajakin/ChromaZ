#include "tmcalc.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace TmCalc {
namespace {

std::string clean(const std::string &s) {
  std::string o;
  for (char c : s) if (!std::isspace(static_cast<unsigned char>(c))) o.push_back((char)std::toupper(static_cast<unsigned char>(c)));
  return o;
}

std::string revComp(const std::string &s) {
  std::string r(s.rbegin(), s.rend());
  for (char &c : r) {
    switch (c) { case 'A': c = 'T'; break; case 'T': c = 'A'; break; case 'C': c = 'G'; break; case 'G': c = 'C'; break; default: break; }
  }
  return r;
}

struct NNParam { double dH; double dS; };

bool lookup(char a, char b, NNParam &p) {
  std::string k; k.push_back(a); k.push_back(b);
  if (k == "AA" || k == "TT") { p = {-7.9, -22.2}; return true; }
  if (k == "AT") { p = {-7.2, -20.4}; return true; }
  if (k == "TA") { p = {-7.2, -21.3}; return true; }
  if (k == "CA" || k == "TG") { p = {-8.5, -22.7}; return true; }
  if (k == "GT" || k == "AC") { p = {-8.4, -22.4}; return true; }
  if (k == "CT" || k == "AG") { p = {-7.8, -21.0}; return true; }
  if (k == "GA" || k == "TC") { p = {-8.2, -22.2}; return true; }
  if (k == "CG") { p = {-10.6, -27.2}; return true; }
  if (k == "GC") { p = {-9.8, -24.4}; return true; }
  if (k == "GG" || k == "CC") { p = {-8.0, -19.9}; return true; }
  return false;
}

} // namespace

double wallace(const std::string &seqIn) {
  std::string s = clean(seqIn);
  int at = 0, gc = 0;
  for (char c : s) { if (c == 'A' || c == 'T') ++at; else if (c == 'G' || c == 'C') ++gc; }
  return 2.0 * at + 4.0 * gc;
}

double gcFormula(const std::string &seqIn) {
  std::string s = clean(seqIn);
  int gc = 0, n = 0;
  for (char c : s) { if (c == 'A' || c == 'T' || c == 'G' || c == 'C') ++n; if (c == 'G' || c == 'C') ++gc; }
  if (n == 0) return 0.0;
  return 64.9 + 41.0 * (gc - 16.4) / n;
}

NNResult nearestNeighbor(const std::string &seqIn, double naMM, double mgMM, double primerNM) {
  NNResult r;
  std::string s = clean(seqIn);
  if (s.size() < 2) { r.reason = "sequence is too short"; return r; }
  for (char c : s) {
    if (c != 'A' && c != 'C' && c != 'G' && c != 'T') { r.reason = "only unambiguous A/C/G/T bases are supported"; return r; }
  }
  if (primerNM <= 0.0) { r.reason = "primer concentration is not set"; return r; }
  double naEq = std::max(0.0, naMM) + 120.0 * std::sqrt(std::max(0.0, mgMM)); // мМ
  if (naEq <= 0.0) { r.reason = "salt concentration is not set"; return r; }

  double dH = 0.0, dS = 0.0; // ккал/моль, кал/(К·моль)
  for (size_t i = 0; i + 1 < s.size(); ++i) {
    NNParam p;
    if (!lookup(s[i], s[i + 1], p)) { r.reason = "unsupported dinucleotide"; return r; }
    dH += p.dH; dS += p.dS;
  }
  auto addEnd = [&](char c) {
    if (c == 'G' || c == 'C') { dH += 0.1; dS += -2.8; }
    else                       { dH += 2.3; dS += 4.1; }
  };
  addEnd(s.front()); addEnd(s.back());

  r.selfComplementary = (s == revComp(s));
  if (r.selfComplementary) dS += -1.4; // поправка на симметрию

  const double N = (double)s.size() - 1.0;         // число фосфатов на цепь
  double dSsalt = dS + 0.368 * N * std::log(naEq / 1000.0);
  const double R = 1.987;                          // кал/(К·моль)
  const double ct = primerNM * 1e-9;               // М
  const double x = r.selfComplementary ? 1.0 : 4.0;
  double denom = dSsalt + R * std::log(ct / x);
  if (denom >= 0.0) { r.reason = "thermodynamic model is not applicable"; return r; }

  r.tm = (dH * 1000.0) / denom - 273.15;
  r.deltaH = dH;
  r.deltaS = dSsalt;
  r.deltaG37 = dH - 310.15 * dSsalt / 1000.0;
  r.naEquivalentMM = naEq;
  r.valid = true;
  return r;
}

} // namespace TmCalc
