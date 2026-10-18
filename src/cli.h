#ifndef CLI_H
#define CLI_H

#include <QCommandLineParser>
#include <QString>

namespace Cli {

// Единое описание параметров командной строки (используется и для разбора, и для справки в программе).
void setupParser(QCommandLineParser &parser);
QString helpText();

// Индекс алгоритма (0..7) по имени (cap3, gotoh, nw, sw, blast, banded, wfa, clustal) или числу; -1 при ошибке.
int parseAlgorithm(const QString &text);
// Индекс таблицы кода (0..2) по имени (standard, mito, bacterial) или числу; -1 при ошибке.
int parseTable(const QString &text);

} // namespace Cli

#endif // CLI_H
