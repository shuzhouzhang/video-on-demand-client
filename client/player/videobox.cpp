// videobox.cpp 实现首页视频卡片组件。
// 当前版本只展示静态数据，点击封面或标题时输出日志，暂不进入播放页。
#include "videobox.h"
#include "ui_videobox.h"
#include "util.h"

#include <QEvent>
#include <QMouseEvent>

VideoBox::VideoBox(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VideoBox)
{
    ui->setupUi(this);

    ui->imageBox->installEventFilter(this);
    ui->videoTitle->installEventFilter(this);
}

VideoBox::~VideoBox()
{
    delete ui;
}

void VideoBox::setVideoInfo(const QString &title,
                            const QString &userName,
                            const QString &date,
                            const QString &duration,
                            const QString &playCount,
                            const QString &likeCount)
{
    m_title = title;
    ui->videoTitle->setText(title);
    ui->userName->setText(userName);
    ui->uploadDate->setText(" · " + date);
    ui->videoDuration->setText(duration);
    ui->playCount->setText(playCount);
    ui->likeCount->setText(likeCount);
}

bool VideoBox::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == ui->imageBox || watched == ui->videoTitle)
        && event->type() == QEvent::MouseButtonPress) {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            LOG() << "点击视频卡片:" << m_title;
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}
