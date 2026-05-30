// main.cpp 是整个客户端程序的入口文件。
// 它先创建 Qt 应用对象，显示启动页，启动页结束后再显示主窗口。
#include "player.h"
#include "startuppage.h"

#include <QApplication>
#include <QGuiApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    // 控制高 DPI 缩放取整策略，减少不同缩放比例下界面尺寸出现小数偏差。
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::Floor);

    QApplication app(argc, argv);

    // 先显示启动页。这里用定时器模拟“启动页停留 2 秒后进入主界面”。
    StartupPage startupPage;
    QTimer::singleShot(2000, &startupPage, &QDialog::accept);
    startupPage.exec();

    // 启动页关闭后再创建并显示主页面。
    player mainWindow;
    mainWindow.show();

    return app.exec();
}
