#include "mainwindow.h"

#include <QApplication>
#include <QSettings>

int main(int argc, char *argv[])
{
    // Without this, qDebug() output is dropped when stderr is a pipe (e.g. Qt Creator)
    if (!qEnvironmentVariableIsSet("QT_ASSUME_STDERR_HAS_CONSOLE"))
        qputenv("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("wuac");
    QCoreApplication::setApplicationName("wuac");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    MainWindow w;
    w.show();
    return a.exec();
}
