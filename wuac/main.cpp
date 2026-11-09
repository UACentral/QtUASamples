#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("QtUASamples");
    QCoreApplication::setApplicationName("wuac");
    MainWindow w;
    w.show();
    return a.exec();
}
