#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <cstdio>
#include "mainwindow.h"
#include "cli.h"

#ifndef CHROMAZ_VERSION
#define CHROMAZ_VERSION "1.0.0"
#endif

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName("ChromaZ");
    QCoreApplication::setApplicationVersion(CHROMAZ_VERSION);

    QCommandLineParser parser;
    Cli::setupParser(parser);
    if (!parser.parse(a.arguments())) {
        const QString msg = parser.errorText() + "\n\n" + Cli::helpText();
        std::fputs(msg.toLocal8Bit().constData(), stderr);
#ifdef Q_OS_WIN   // у GUI-приложения Windows нет консоли
        QMessageBox::critical(nullptr, "ChromaZ", msg);
#endif
        return 2;
    }
    if (parser.isSet("help") || parser.isSet("version")) {
        const QString text = parser.isSet("help") ? Cli::helpText()
                                                  : QString("ChromaZ %1\n").arg(CHROMAZ_VERSION);
        std::fputs(text.toLocal8Bit().constData(), stdout);
#ifdef Q_OS_WIN
        QMessageBox::information(nullptr, "ChromaZ", text);
#endif
        return 0;
    }

    MainWindow w;
    w.show();
    if (parser.positionalArguments().isEmpty() && parser.optionNames().isEmpty())
        w.maybeRestoreAutosave();
    else
        w.applyCommandLine(parser);
    return a.exec();
}
