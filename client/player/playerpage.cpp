// playerpage.cpp 实现视频播放页的静态 UI 和基础交互。
// 播放页已接入 libmpv，负责播放控制、时间同步和视频信息展示。
#include "playerpage.h"
#include "apiclient.h"
#include "bulletscreenitem.h"
#include "datacenter.h"
#include "ui_playerpage.h"
#include "util.h"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QShortcut>
#include <QShowEvent>
#include <QSlider>
#include <QSize>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QStringList>
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
    if (m_barrageLayer) {
        m_barrageLayer->hide();
    }
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
        updateBarrageLayerGeometry();
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

void PlayerPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateBarrageLayerGeometry();
    if (m_isBarrageEnabled && m_barrageLayer) {
        m_barrageLayer->show();
        m_barrageLayer->raise();
    }
}

void PlayerPage::hideEvent(QHideEvent *event)
{
    if (m_barrageLayer) {
        m_barrageLayer->hide();
    }
    QWidget::hideEvent(event);
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
    m_apiClient = new ApiClient(this);

    initSpeedMenu();
    initVolumePanel();
    initBarrageLayer();
    initBarrageControls();
    updatePlayButton();
    updateLikeButton();
    updateSpeedButton();
    updateVolumeLabel();
    updateBarrageButton();

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

        const int targetSeconds = sliderValueToSeconds();
        m_currentPlaySeconds = targetSeconds;
        m_mpvPlayer->setCurrentPlayPosition(targetSeconds);
        updateTimeLabel(targetSeconds);
        updateSliderPosition(targetSeconds);
    });

    connect(ui->videoSlider, &QSlider::valueChanged, this, [this](int value) {
        if (!m_isSliderPressed || m_durationSeconds <= 0) {
            return;
        }

        updateTimeLabel(value * m_durationSeconds / ui->videoSlider->maximum());
    });

    connect(m_mpvPlayer, &MpvPlayer::durationChanged, this, [this](int durationSeconds) {
        if (durationSeconds <= 0) {
            return;
        }

        m_durationSeconds = durationSeconds;
        updateTimeLabel(0);
    });

    connect(m_mpvPlayer, &MpvPlayer::playPositionChanged, this, [this](int currentSeconds) {
        m_currentPlaySeconds = currentSeconds;
        updateTimeLabel(currentSeconds);
        updateSliderPosition(currentSeconds);
        showBarragesAt(currentSeconds);
    });

    connect(m_mpvPlayer, &MpvPlayer::endOfPlaylist, this, [this]() {
        m_isSliderPressed = false;
        m_isPlaying = false;
        m_currentPlaySeconds = m_durationSeconds;
        updatePlayButton();
        updateSliderPosition(m_durationSeconds);
        updateTimeLabel(m_durationSeconds);
    });

    connect(m_apiClient, &ApiClient::playUrlLoaded, this, [this](const QString &playUrl) {
        // 这是什么：播放页拿到接口返回的播放地址后启动 mpv。
        // 为什么能实现：ApiClient 已确认 playUrl 非空，MpvPlayer::startPlay() 可以直接加载本地路径或后续网络地址。
        // 什么时候调用：GET /videos/play-url 成功返回时由 Qt 信号槽触发。
        // 和谁配合：ApiClient 负责请求地址，startPlayback() 负责统一设置 m_videoKey 并启动播放器。
        startPlayback(playUrl);
    });
    connect(m_apiClient, &ApiClient::playUrlFailed, this, [this](const QString &message) {
        // 这是什么：播放地址接口失败后的本地回退。
        // 为什么能实现：当前阶段仍保留 test.mp4，本地路径可保证 mock server 关闭时播放页不至于空白。
        // 什么时候调用：网络错误、接口返回失败或 playUrl 为空时触发。
        // 和谁配合：ApiClient 发失败信号，startPlayback() 继续启动本地测试视频。
        LOG() << "播放地址接口请求失败，回退本地测试视频:" << message;
        startPlayback("D:/video-on-demand-client/test.mp4");
    });
    m_apiClient->fetchPlayUrl();

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

void PlayerPage::initBarrageLayer()
{
    m_barrageLayer = new QWidget(this, Qt::FramelessWindowHint | Qt::Tool);
    m_barrageLayer->setAttribute(Qt::WA_TranslucentBackground);
    m_barrageLayer->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_barrageLayer->setObjectName("barrageLayer");
    m_barrageLayer->setStyleSheet(R"(
        QWidget#barrageLayer {
            background: transparent;
        }
        QFrame#barrageTrack {
            border: none;
            background: transparent;
        }
    )");

    auto *layout = new QVBoxLayout(m_barrageLayer);
    layout->setContentsMargins(0, 16, 0, 0);
    layout->setSpacing(6);

    m_barrageTrackTop = new QFrame(m_barrageLayer);
    m_barrageTrackMiddle = new QFrame(m_barrageLayer);
    m_barrageTrackBottom = new QFrame(m_barrageLayer);
    for (QFrame *track : {m_barrageTrackTop, m_barrageTrackMiddle, m_barrageTrackBottom}) {
        track->setObjectName("barrageTrack");
        track->setMinimumHeight(34);
        track->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        layout->addWidget(track);
    }
    layout->addStretch(1);

    updateBarrageLayerGeometry();
    m_barrageLayer->hide();
}

void PlayerPage::initBarrageControls()
{
    m_barrageToggleBtn = new QPushButton(ui->playControl);
    m_barrageToggleBtn->setFixedSize(28, 28);
    m_barrageToggleBtn->setCursor(Qt::PointingHandCursor);
    m_barrageToggleBtn->setToolTip(QString::fromUtf8("弹幕开关"));
    m_barrageToggleBtn->setStyleSheet(R"(
        QPushButton {
            border: none;
            background: transparent;
        }
        QPushButton:hover {
            background: rgba(255, 255, 255, 0.10);
        }
    )");

    m_barrageEdit = new QLineEdit(ui->playControl);
    m_barrageEdit->setMaxLength(30);
    m_barrageEdit->setPlaceholderText(QString::fromUtf8("发个友善的弹幕..."));
    m_barrageEdit->setFixedHeight(28);
    m_barrageEdit->setMinimumWidth(180);
    m_barrageEdit->setMaximumWidth(360);
    m_barrageEdit->setStyleSheet(R"(
        QLineEdit {
            border: 1px solid #374151;
            border-radius: 4px;
            padding: 0 10px;
            background: #111827;
            color: #e5e7eb;
            selection-background-color: #3eceff;
            font-size: 13px;
        }
        QLineEdit:focus {
            border-color: #3eceff;
        }
    )");

    m_barrageSendBtn = new QPushButton(QString::fromUtf8("发送"), ui->playControl);
    m_barrageSendBtn->setFixedSize(56, 28);
    m_barrageSendBtn->setCursor(Qt::PointingHandCursor);
    m_barrageSendBtn->setStyleSheet(R"(
        QPushButton {
            border: none;
            border-radius: 4px;
            background: #3eceff;
            color: #ffffff;
            font-size: 13px;
            font-weight: 500;
        }
        QPushButton:hover {
            background: #23b6e6;
        }
    )");

    ui->buttonLayout->insertWidget(2, m_barrageToggleBtn);
    ui->buttonLayout->insertWidget(3, m_barrageEdit, 1);
    ui->buttonLayout->insertWidget(4, m_barrageSendBtn);

    connect(m_barrageToggleBtn, &QPushButton::clicked, this, [this]() {
        m_isBarrageEnabled = !m_isBarrageEnabled;
        if (m_barrageLayer) {
            m_barrageLayer->setVisible(m_isBarrageEnabled && isVisible());
            if (m_isBarrageEnabled) {
                updateBarrageLayerGeometry();
                m_barrageLayer->raise();
            }
        }
        updateBarrageButton();
    });

    connect(m_barrageSendBtn, &QPushButton::clicked, this, &PlayerPage::sendBarrage);
    connect(m_barrageEdit, &QLineEdit::returnPressed, this, &PlayerPage::sendBarrage);
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

void PlayerPage::updateBarrageButton()
{
    if (!m_barrageToggleBtn) {
        return;
    }

    m_barrageToggleBtn->setIcon(QIcon(m_isBarrageEnabled
                                          ? ":/images/PlayPage/danmu.png"
                                          : ":/images/PlayPage/danmuguan.png"));
    m_barrageToggleBtn->setIconSize(QSize(22, 22));
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

void PlayerPage::updateBarrageLayerGeometry()
{
    if (!m_barrageLayer || !ui || !ui->videoScreen) {
        return;
    }

    m_barrageLayer->setFixedSize(ui->videoScreen->size());
    m_barrageLayer->move(ui->videoScreen->mapToGlobal(QPoint(0, 0)));
}

void PlayerPage::updateTimeLabel(int currentSeconds)
{
    if (m_durationSeconds > 0 && currentSeconds > m_durationSeconds) {
        currentSeconds = m_durationSeconds;
    }

    ui->timeLabel->setText(formatSeconds(currentSeconds) + " / " + formatSeconds(m_durationSeconds));
}

void PlayerPage::updateSliderPosition(int currentSeconds)
{
    if (m_isSliderPressed || m_durationSeconds <= 0) {
        return;
    }

    const QSignalBlocker blocker(ui->videoSlider);
    currentSeconds = qBound(0, currentSeconds, m_durationSeconds);
    ui->videoSlider->setValue(currentSeconds * ui->videoSlider->maximum() / m_durationSeconds);
}

int PlayerPage::sliderValueToSeconds() const
{
    if (m_durationSeconds <= 0 || ui->videoSlider->maximum() <= 0) {
        return 0;
    }

    return qBound(0, ui->videoSlider->value() * m_durationSeconds / ui->videoSlider->maximum(), m_durationSeconds);
}

void PlayerPage::sendBarrage()
{
    if (!m_barrageEdit) {
        return;
    }

    const QString text = m_barrageEdit->text().trimmed();
    if (text.isEmpty()) {
        return;
    }

    const int upperBound = m_durationSeconds > 0 ? m_durationSeconds : m_currentPlaySeconds;
    const int second = qBound(0, m_currentPlaySeconds, upperBound);
    DataCenter::instance().addBarrage(m_videoKey, second, text);

    if (m_isBarrageEnabled) {
        showBarrageText(text, 0);
        m_triggeredBarrageSeconds.insert(second);
    }

    m_barrageEdit->clear();
}

void PlayerPage::showBarragesAt(int seconds)
{
    if (!m_isBarrageEnabled || seconds < 0 || m_triggeredBarrageSeconds.contains(seconds)) {
        return;
    }

    m_triggeredBarrageSeconds.insert(seconds);
    const QStringList barrages = DataCenter::instance().barragesAt(m_videoKey, seconds);
    for (const QString &text : barrages) {
        showBarrageText(text);
    }
}

void PlayerPage::showBarrageText(const QString &text, int trackIndex)
{
    if (!m_barrageLayer || text.trimmed().isEmpty()) {
        return;
    }

    updateBarrageLayerGeometry();
    if (!m_barrageLayer->isVisible()) {
        m_barrageLayer->show();
    }
    m_barrageLayer->raise();

    if (trackIndex < 0) {
        trackIndex = m_nextBarrageTrack % 3;
        ++m_nextBarrageTrack;
    }

    QFrame *track = barrageTrackForIndex(trackIndex);
    if (!track || track->width() <= 0) {
        return;
    }

    auto *item = new BulletScreenItem(track);
    item->setBulletScreenText(text.left(30));
    const int startX = track->width();
    const int durationMs = qMax(6500, (startX + item->width()) * 9);
    item->setBulletScreenAnimation(startX, durationMs);
    item->startAnimation();
}

QFrame *PlayerPage::barrageTrackForIndex(int index) const
{
    switch (index % 3) {
    case 0:
        return m_barrageTrackTop;
    case 1:
        return m_barrageTrackMiddle;
    default:
        return m_barrageTrackBottom;
    }
}

void PlayerPage::startPlayback(const QString &playUrl)
{
    // 这是什么：用指定播放地址启动 mpv，并保持进入播放页后默认暂停。
    // 为什么能实现：MpvPlayer 已经绑定到 videoScreen，startPlay() 加载地址后 pause() 可以停在初始状态。
    // 什么时候调用：播放地址接口成功返回，或接口失败需要回退本地 test.mp4 时调用。
    // 和谁配合：ApiClient 提供 playUrl，MpvPlayer 负责实际播放，弹幕用 m_videoKey 区分视频。
    const QString trimmedPlayUrl = playUrl.trimmed();
    if (trimmedPlayUrl.isEmpty()) {
        return;
    }

    m_videoKey = trimmedPlayUrl;
    m_mpvPlayer->startPlay(m_videoKey);
    m_mpvPlayer->pause();
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
