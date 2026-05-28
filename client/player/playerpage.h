// playerpage.h 声明视频播放页组件。
// PlayerPage 是一个独立窗口，第一版只承载播放页静态壳子和基础按钮行为。
#ifndef PLAYERPAGE_H
#define PLAYERPAGE_H

#include <QPoint>
#include <QString>
#include <QWidget>

class QLabel;
class QMenu;
class QSlider;

namespace Ui {
class PlayerPage;
}

class PlayerPage : public QWidget
{
    Q_OBJECT

public:
    explicit PlayerPage(const QString &title,
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

private:
    void initUI(const QString &title,
                const QString &userName,
                const QString &date,
                const QString &duration,
                const QString &playCount,
                const QString &likeCount);
    void initSpeedMenu();
    void initVolumePanel();
    void updatePlayButton();
    void updateLikeButton();
    void updateSpeedButton();
    void updateVolumeLabel();
    void showVolumePanel();

private:
    Ui::PlayerPage *ui;
    bool m_isDragging = false;
    bool m_isPlaying = false;
    bool m_isLiked = false;
    double m_playSpeed = 1.0;
    int m_volume = 60;
    QPoint m_dragOffset;
    QString m_title;
    QMenu *m_speedMenu = nullptr;
    QWidget *m_volumePanel = nullptr;
    QSlider *m_volumeSlider = nullptr;
    QLabel *m_volumeValueLabel = nullptr;
};

#endif // PLAYERPAGE_H
