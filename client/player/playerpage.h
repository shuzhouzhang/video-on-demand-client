// playerpage.h 声明视频播放页组件。
// PlayerPage 是一个独立窗口，第一版只承载播放页静态壳子和基础按钮行为。
#ifndef PLAYERPAGE_H
#define PLAYERPAGE_H

#include <QPoint>
#include <QSet>
#include <QString>
#include <QWidget>

#include "mpv/mpvplayer.h"

class QLabel;
class QFrame;
class QHideEvent;
class QLineEdit;
class QMenu;
class QPushButton;
class QShowEvent;
class QSlider;
class QTimer;
class ApiClient;
struct VideoInfo;

namespace Ui {
class PlayerPage;
}

class PlayerPage : public QWidget
{
    Q_OBJECT

public:
    explicit PlayerPage(const QString &videoId,
                        const QString &title,
                        const QString &userName,
                        const QString &date,
                        const QString &duration,
                        const QString &playCount,
                        const QString &likeCount,
                        QWidget *parent = nullptr);
    ~PlayerPage() override;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void initUI(const QString &videoId,
                const QString &title,
                const QString &userName,
                const QString &date,
                const QString &duration,
                const QString &playCount,
                const QString &likeCount);
    void initSpeedMenu();
    void initVolumePanel();
    void initBarrageLayer();
    void initBarrageControls();
    void updatePlayButton();
    void updateLikeButton();
    void updateSpeedButton();
    void updateVolumeLabel();
    void updateBarrageButton();
    void showVolumePanel();
    void updateBarrageLayerGeometry();
    void updateTimeLabel(int currentSeconds);
    void updateSliderPosition(int currentSeconds);
    int sliderValueToSeconds() const;
    void sendBarrage();
    void showBarragesAt(int seconds);
    void showBarrageText(const QString &text, int trackIndex = -1);
    QFrame *barrageTrackForIndex(int index) const;
    void startPlayback(const QString &playUrl);
    void applyVideoDetail(const VideoInfo &video);
    void applyPendingWatchProgress();
    void submitWatchProgress();
    static QString formatSeconds(int seconds);

private:
    Ui::PlayerPage *ui;
    bool m_isDragging = false;
    bool m_isPlaying = false;
    bool m_isLiked = false;
    bool m_isSliderPressed = false;
    double m_playSpeed = 1.0;
    int m_volume = 60;
    int m_durationSeconds = 0;
    int m_currentPlaySeconds = 0;
    int m_pendingSeekSeconds = -1;
    int m_nextBarrageTrack = 0;
    QPoint m_dragOffset;
    QString m_videoId;
    QString m_title;
    QString m_videoKey;
    QMenu *m_speedMenu = nullptr;
    QWidget *m_volumePanel = nullptr;
    QSlider *m_volumeSlider = nullptr;
    QLabel *m_volumeValueLabel = nullptr;
    ApiClient *m_apiClient = nullptr;
    MpvPlayer *m_mpvPlayer = nullptr;
    QTimer *m_watchProgressTimer = nullptr;
    bool m_isBarrageEnabled = true;
    QSet<int> m_triggeredBarrageSeconds;
    QWidget *m_barrageLayer = nullptr;
    QFrame *m_barrageTrackTop = nullptr;
    QFrame *m_barrageTrackMiddle = nullptr;
    QFrame *m_barrageTrackBottom = nullptr;
    QPushButton *m_barrageToggleBtn = nullptr;
    QLineEdit *m_barrageEdit = nullptr;
    QPushButton *m_barrageSendBtn = nullptr;
};

#endif // PLAYERPAGE_H
