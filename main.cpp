#include "mainwindow.h"

#include <QIcon>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QApplication::setApplicationName("memyze");
    QApplication::setDesktopFileName("memyze");

    QIcon appIcon(":/assets/logo.png");
    a.setWindowIcon(appIcon);

    MainWindow w;
    w.show();
    return a.exec();
}
