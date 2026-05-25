#include "startuppage.h"

#include <QFile>
#include <QLabel>
#include <QPixmap>

StartupPage::StartupPage(QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::Tool);
    setWindowModality(Qt::ApplicationModal);
    setFixedSize(1450, 860);
    setObjectName("startupPage");
    setStyleSheet("QDialog#startupPage { background-color: #ffffff; }");

    auto *logoLabel = new QLabel(this);
    logoLabel->move(524, 374);
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setStyleSheet("background: transparent;");

    QPixmap logo(":/images/startupPage/biteshipin.png");
    if (logo.isNull()) {
        logo.load("D:/video-on-demand-client/client/player/images/startupPage/biteshipin.png");
    }

    if (logo.isNull()) {
        logoLabel->setFixedSize(420, 120);
        logoLabel->setStyleSheet("color: red; font-size: 18px; background: transparent;");
        logoLabel->setText("logo load failed");
        return;
    }

    logoLabel->setFixedSize(logo.size());
    logoLabel->setPixmap(logo);
}
