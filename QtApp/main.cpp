#include "QtPCGToVSTUI.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QtPCGToVSTUI w;
    w.show();
    return a.exec();
}
