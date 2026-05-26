// startuppage.cpp 实现启动页界面。
// 当前启动页固定为 1450x860，居中显示“比特视频”启动 logo。
#include "startuppage.h"
#include "util.h"

#include <QFile>
#include <QLabel>
#include <QPixmap>

StartupPage::StartupPage(QWidget *parent)
    : QDialog(parent)
{
    // 启动页使用无边框模态窗口，避免用户在启动页期间操作后面的主窗口。
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::Tool);
    setWindowModality(Qt::ApplicationModal);
    setFixedSize(1450, 860);
    setObjectName("startupPage");
    setStyleSheet("QDialog#startupPage { background-color: #ffffff; }");

    auto *logoLabel = new QLabel(this);
    // 这里的位置来自参考图尺寸，配合固定窗口大小让 logo 位于启动页中间区域。
    logoLabel->move(524, 374);
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setStyleSheet("background: transparent;");

    QPixmap logo(":/images/startupPage/biteshipin.png");
    if (logo.isNull()) {
        // 兜底路径：如果资源文件没有编译进去，开发环境下还能从磁盘直接加载图片。
        LOG() << "启动页资源图片加载失败，尝试从磁盘路径加载";
        logo.load("D:/video-on-demand-client/client/player/images/startupPage/biteshipin.png");
    }

    if (logo.isNull()) {
        // 两种方式都失败时，直接显示错误文字，方便定位“图片没加载出来”的问题。
        LOG() << "启动页 logo 加载失败";
        logoLabel->setFixedSize(420, 120);
        logoLabel->setStyleSheet("color: red; font-size: 18px; background: transparent;");
        logoLabel->setText("logo load failed");
        return;
    }

    logoLabel->setFixedSize(logo.size());
    logoLabel->setPixmap(logo);
}
