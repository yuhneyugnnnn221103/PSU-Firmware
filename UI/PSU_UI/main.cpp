#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);

    QApplication::setApplicationName(QStringLiteral("INA228 Monitor"));
    QApplication::setOrganizationName(QStringLiteral("PMON"));

    MainWindow w;
    w.show();
    return QApplication::exec();
}
