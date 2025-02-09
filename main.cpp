#include "mainwindowcodeeditor.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindowCodeEditor w;
    w.show();
    return a.exec();
}
