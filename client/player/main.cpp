#include "player.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    player w;
    w.show();
    return QCoreApplication::exec();
}
