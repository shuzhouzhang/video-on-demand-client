// videobox.cpp 实现首页视频卡片组件。
// 当前版本展示 VideoInfo 数据，点击封面或标题时发出带 videoId 的信号，由主窗口打开播放页。
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

void VideoBox::setVideoInfo(const QString &id,
                            const QString &title,
                            const QString &userName,
                            const QString &date,
                            const QString &duration,
                            const QString &playCount,
                            const QString &likeCount)
{
    m_id = id;
    m_title = title;
    m_userName = userName;
    m_date = date;
    m_duration = duration;
    m_playCount = playCount;
    m_likeCount = likeCount;

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
            emit videoClicked(m_id, m_title, m_userName, m_date, m_duration, m_playCount, m_likeCount);
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}
