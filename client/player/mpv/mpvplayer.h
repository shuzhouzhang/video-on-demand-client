#ifndef MPVPLAYER_H
#define MPVPLAYER_H

#include "client.h"

#include <QObject>
#include <QString>

class QWidget;

class MpvPlayer : public QObject
{
    Q_OBJECT

public:
    explicit MpvPlayer(QWidget *videoRenderWidget = nullptr, QObject *parent = nullptr);
    ~MpvPlayer() override;

    void startPlay(const QString &videoPath);
    void play();
    void pause();
    void setPlaySpeed(double speed);
    void setVolume(int volume);
    void setCurrentPlayPosition(int seconds);

signals:
    void mpvEvents();
    void playPositionChanged(int seconds);
    void durationChanged(int seconds);
    void endOfPlaylist();

private slots:
    void onMpvEvents();

private:
    void handleMpvEvent(mpv_event *event);

private:
    mpv_handle *m_mpv = nullptr;
    int m_currentSeconds = -1;
};

#endif // MPVPLAYER_H
