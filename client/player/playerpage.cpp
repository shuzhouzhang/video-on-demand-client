// playerpage.cpp 实现视频播放页的静态 UI 和基础交互。
// 当前阶段不接真实播放器，只验证播放页窗口、数据展示和按钮事件链路。
#include "playerpage.h"
#include "ui_playerpage.h"
#include "util.h"

#include <QAction>
#include <QFrame>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QShortcut>
#include <QSlider>
#include <QSize>
#include <QSignalBlocker>
#include <QVBoxLayout>

PlayerPage::PlayerPage(const QString &title,
                       const QString &userName,
                       const QString &date,
                       const QString &duration,
                       const QString &playCount,
                       const QString &likeCount,
                       QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlayerPage)
{
    initUI(title, userName, date, duration, playCount, likeCount);
}

PlayerPage::~PlayerPage()
{
    delete m_mpvPlayer;
    m_mpvPlayer = nullptr;
    delete ui;
}

void PlayerPage::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && ui->playHead->geometry().contains(event->position().toPoint())) {
        m_isDragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void PlayerPage::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void PlayerPage::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void PlayerPage::initUI(const QString &title,
                        const QString &userName,
                        const QString &date,
                        const QString &duration,
                        const QString &playCount,
                        const QString &likeCount)
{
    ui->setupUi(this);

    m_title = title;
    setFixedSize(1450, 860);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setWindowTitle(title);

    ui->videoTitle->setText(title);
    ui->userName->setText(userName);
    ui->uploadDate->setText(date);
    ui->playNum->setText(playCount);
    ui->likeNum->setText(likeCount);
    ui->timeLabel->setText("00:00 / " + duration);
    ui->videoSlider->setRange(0, 1000);
    ui->videoSlider->setValue(0);
    ui->videoDesc->setText("简介：这是一条视频简介占位内容，后续接入真实视频数据后会展示作者填写的视频说明。");

    m_mpvPlayer = new MpvPlayer(ui->videoScreen, this);

    initSpeedMenu();
    initVolumePanel();
    updatePlayButton();
    updateLikeButton();
    updateSpeedButton();
    updateVolumeLabel();

    auto *playShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    playShortcut->setContext(Qt::WindowShortcut);
    connect(playShortcut, &QShortcut::activated, ui->playBtn, &QPushButton::click);

    connect(ui->minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(ui->quitBtn, &QPushButton::clicked, this, &QWidget::close);

    connect(ui->playBtn, &QPushButton::clicked, this, [this]() {
        m_isPlaying = !m_isPlaying;
        if (m_isPlaying) {
            m_mpvPlayer->play();
        } else {
            m_mpvPlayer->pause();
        }
        updatePlayButton();
        LOG() << "播放页切换播放状态:" << m_title << (m_isPlaying ? "播放" : "暂停");
    });

    connect(ui->likeBtn, &QPushButton::clicked, this, [this]() {
        m_isLiked = !m_isLiked;
        updateLikeButton();
        LOG() << "播放页切换点赞状态:" << m_title << (m_isLiked ? "已点赞" : "取消点赞");
    });

    connect(ui->speedBtn, &QPushButton::clicked, this, [this]() {
        const QPoint menuPos = ui->speedBtn->mapToGlobal(QPoint(0, -m_speedMenu->sizeHint().height() - 8));
        m_speedMenu->popup(menuPos);
    });

    connect(ui->volumeBtn, &QPushButton::clicked, this, [this]() {
        showVolumePanel();
    });

    connect(ui->videoSlider, &QSlider::sliderPressed, this, [this]() {
        m_isSliderPressed = true;
    });

    connect(ui->videoSlider, &QSlider::sliderReleased, this, [this]() {
        m_isSliderPressed = false;
        if (m_durationSeconds <= 0) {
            return;
        }

        const int targetSeconds = ui->videoSlider->value() * m_durationSeconds / ui->videoSlider->maximum();
        m_mpvPlayer->setCurrentPlayPosition(targetSeconds);
        updateTimeLabel(targetSeconds);
    });

    connect(ui->videoSlider, &QSlider::valueChanged, this, [this](int value) {
        if (!m_isSliderPressed || m_durationSeconds <= 0) {
            return;
        }

        const int targetSeconds = value * m_durationSeconds / ui->videoSlider->maximum();
        updateTimeLabel(targetSeconds);
    });

    connect(m_mpvPlayer, &MpvPlayer::durationChanged, this, [this](int durationSeconds) {
        m_durationSeconds = durationSeconds;
        updateTimeLabel(0);
    });

    connect(m_mpvPlayer, &MpvPlayer::playPositionChanged, this, [this](int currentSeconds) {
        updateTimeLabel(currentSeconds);
        updateSliderPosition(currentSeconds);
    });

    connect(m_mpvPlayer, &MpvPlayer::endOfPlaylist, this, [this]() {
        m_isPlaying = false;
        updatePlayButton();
        updateSliderPosition(m_durationSeconds);
        updateTimeLabel(m_durationSeconds);
    });

    m_mpvPlayer->startPlay("D:/video-on-demand-client/test.mp4");
    m_mpvPlayer->pause();
    m_mpvPlayer->setVolume(m_volume);
    m_mpvPlayer->setPlaySpeed(m_playSpeed);
}

void PlayerPage::initSpeedMenu()
{
    m_speedMenu = new QMenu(this);
    m_speedMenu->setStyleSheet(R"(
        QMenu {
            padding: 6px;
            border: 1px solid #20242a;
            border-radius: 6px;
            background: #111827;
            color: #e5e7eb;
            font-size: 14px;
        }
        QMenu::item {
            min-width: 72px;
            padding: 7px 14px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background: #1f2937;
            color: #3eceff;
        }
    )");

    const QList<double> speeds = {0.5, 1.0, 1.25, 1.5, 2.0};
    for (double speed : speeds) {
        auto *action = m_speedMenu->addAction(QString::number(speed, 'f', speed == 1.25 ? 2 : 1) + "x");
        connect(action, &QAction::triggered, this, [this, speed]() {
            m_playSpeed = speed;
            m_mpvPlayer->setPlaySpeed(m_playSpeed);
            updateSpeedButton();
            LOG() << "播放页切换倍速:" << m_title << m_playSpeed;
        });
    }
}

void PlayerPage::initVolumePanel()
{
    auto *panel = new QFrame(this);
    panel->setObjectName("volumePanel");
    panel->setFixedSize(64, 178);
    panel->setStyleSheet(R"(
        QFrame#volumePanel {
            border: 1px solid #20242a;
            border-radius: 8px;
            background: #111827;
        }
        QLabel#volumeValueLabel {
            color: #e5e7eb;
            font-size: 12px;
        }
        QSlider::groove:vertical {
            width: 4px;
            border-radius: 2px;
            background: #374151;
        }
        QSlider::sub-page:vertical {
            border-radius: 2px;
            background: #374151;
        }
        QSlider::add-page:vertical {
            border-radius: 2px;
            background: #3eceff;
        }
        QSlider::handle:vertical {
            width: 14px;
            height: 14px;
            margin: 0 -5px;
            border-radius: 7px;
            background: #3eceff;
        }
    )");

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 10, 0, 8);
    layout->setSpacing(6);

    m_volumeSlider = new QSlider(Qt::Vertical, panel);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(m_volume);
    m_volumeSlider->setInvertedAppearance(false);
    m_volumeSlider->setInvertedControls(false);
    m_volumeSlider->setCursor(Qt::PointingHandCursor);
    layout->addWidget(m_volumeSlider, 1, Qt::AlignHCenter);

    m_volumeValueLabel = new QLabel(panel);
    m_volumeValueLabel->setObjectName("volumeValueLabel");
    m_volumeValueLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_volumeValueLabel);

    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_volume = value;
        if (m_mpvPlayer) {
            m_mpvPlayer->setVolume(m_volume);
        }
        updateVolumeLabel();
        LOG() << "播放页调整音量:" << m_title << m_volume;
    });

    m_volumePanel = panel;
    m_volumePanel->hide();
}

void PlayerPage::updatePlayButton()
{
    ui->playBtn->setText(QString());
    ui->playBtn->setIcon(QIcon(m_isPlaying
                                   ? ":/images/PlayPage/bofang.png"
                                   : ":/images/PlayPage/zanting.png"));
    ui->playBtn->setIconSize(QSize(24, 24));
    ui->playBtn->setStyleSheet(R"(
        QPushButton#playBtn {
            border: none;
            background: transparent;
        }
        QPushButton#playBtn:hover {
            background: rgba(255, 255, 255, 0.10);
        }
    )");
}

void PlayerPage::updateLikeButton()
{
    ui->likeBtn->setText(QString::fromUtf8("👍"));
    ui->likeBtn->setIcon(QIcon());
    if (m_isLiked) {
        ui->likeBtn->setStyleSheet(R"(
            QPushButton#likeBtn {
                border: none;
                border-radius: 12px;
                background: #3eceff;
                color: #ffffff;
                font-size: 15px;
            }
            QPushButton#likeBtn:hover {
                background: #23b6e6;
            }
        )");
    } else {
        ui->likeBtn->setStyleSheet(R"(
            QPushButton#likeBtn {
                border: none;
                background: transparent;
                color: #6b7280;
                font-size: 15px;
            }
            QPushButton#likeBtn:hover {
                background: transparent;
                color: #3eceff;
            }
        )");
    }

    ui->likeNum->setStyleSheet(QString("color: %1; font-size: 14px;")
                                   .arg(m_isLiked ? "#3eceff" : "#6b7280"));
}

void PlayerPage::updateSpeedButton()
{
    ui->speedBtn->setText(QString::number(m_playSpeed, 'f', m_playSpeed == 1.25 ? 2 : 1) + "x");
}

void PlayerPage::updateVolumeLabel()
{
    if (m_volumeValueLabel) {
        m_volumeValueLabel->setText(QString::number(m_volume));
    }
}

void PlayerPage::showVolumePanel()
{
    if (m_volumePanel->isVisible()) {
        m_volumePanel->hide();
        return;
    }

    const QPoint panelPos = ui->volumeBtn->mapTo(this, QPoint((ui->volumeBtn->width() - m_volumePanel->width()) / 2,
                                                             -m_volumePanel->height() - 10));
    m_volumePanel->move(panelPos);
    m_volumePanel->show();
    m_volumePanel->raise();
}

void PlayerPage::updateTimeLabel(int currentSeconds)
{
    ui->timeLabel->setText(formatSeconds(currentSeconds) + " / " + formatSeconds(m_durationSeconds));
}

void PlayerPage::updateSliderPosition(int currentSeconds)
{
    if (m_isSliderPressed || m_durationSeconds <= 0) {
        return;
    }

    const QSignalBlocker blocker(ui->videoSlider);
    ui->videoSlider->setValue(currentSeconds * ui->videoSlider->maximum() / m_durationSeconds);
}

QString PlayerPage::formatSeconds(int seconds)
{
    if (seconds < 0) {
        seconds = 0;
    }

    const int hours = seconds / 3600;
    const int minutes = seconds % 3600 / 60;
    const int remainingSeconds = seconds % 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(remainingSeconds, 2, 10, QLatin1Char('0'));
    }

    return QString("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(remainingSeconds, 2, 10, QLatin1Char('0'));
}
