#include "player.h"
#include "startuppage.h"

#include <QApplication>
#include <QGuiApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::Floor);

    QApplication app(argc, argv);

    StartupPage startupPage;
    QTimer::singleShot(2000, &startupPage, &QDialog::accept);
    startupPage.exec();

    player mainWindow;
    mainWindow.show();

    return app.exec();
}
