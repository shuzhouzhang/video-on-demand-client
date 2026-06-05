#include "mpvplayer.h"
#include "util.h"

#include <QByteArray>
#include <QWidget>

#include <algorithm>
#include <clocale>
#include <cstring>

namespace {
void wakeup(void *ctx)
{
    auto *player = static_cast<MpvPlayer *>(ctx);
    emit player->mpvEvents();
}
}

MpvPlayer::MpvPlayer(QWidget *videoRenderWidget, QObject *parent)
    : QObject(parent)
{
    std::setlocale(LC_NUMERIC, "C");

    m_mpv = mpv_create();
    if (m_mpv == nullptr) {
        LOG() << "mpv instance create failed";
        return;
    }

    if (videoRenderWidget != nullptr) {
        int64_t wid = static_cast<int64_t>(videoRenderWidget->winId());
        if (mpv_set_option(m_mpv, "wid", MPV_FORMAT_INT64, &wid) < 0) {
            LOG() << "mpv set wid failed";
        }
    } else {
        mpv_set_option_string(m_mpv, "vo", "null");
        mpv_set_option_string(m_mpv, "ao", "null");
    }

    if (mpv_initialize(m_mpv) < 0) {
        LOG() << "mpv initialize failed";
        mpv_destroy(m_mpv);
        m_mpv = nullptr;
        return;
    }

    mpv_observe_property(m_mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "duration", MPV_FORMAT_DOUBLE);

    connect(this, &MpvPlayer::mpvEvents, this, &MpvPlayer::onMpvEvents, Qt::QueuedConnection);
    mpv_set_wakeup_callback(m_mpv, wakeup, this);
}

MpvPlayer::~MpvPlayer()
{
    if (m_mpv != nullptr) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
}

void MpvPlayer::startPlay(const QString &videoPath)
{
    if (m_mpv == nullptr) {
        return;
    }

    m_currentSeconds = -1;
    const QByteArray fileName = videoPath.toUtf8();
    const char *args[] = {"loadfile", fileName.constData(), nullptr};
    mpv_command_async(m_mpv, 0, args);
}

void MpvPlayer::play()
{
    if (m_mpv == nullptr) {
        return;
    }

    int pause = 0;
    mpv_set_property_async(m_mpv, 0, "pause", MPV_FORMAT_FLAG, &pause);
}

void MpvPlayer::pause()
{
    if (m_mpv == nullptr) {
        return;
    }

    int pause = 1;
    mpv_set_property_async(m_mpv, 0, "pause", MPV_FORMAT_FLAG, &pause);
}

void MpvPlayer::setPlaySpeed(double speed)
{
    if (m_mpv == nullptr) {
        return;
    }

    mpv_set_property_async(m_mpv, 0, "speed", MPV_FORMAT_DOUBLE, &speed);
}

void MpvPlayer::setVolume(int volume)
{
    if (m_mpv == nullptr) {
        return;
    }

    int64_t mpvVolume = volume;
    mpv_set_property_async(m_mpv, 0, "volume", MPV_FORMAT_INT64, &mpvVolume);
}

void MpvPlayer::setCurrentPlayPosition(int seconds)
{
    if (m_mpv == nullptr) {
        return;
    }

    double targetSeconds = std::max(0, seconds);
    mpv_set_property_async(m_mpv, 0, "time-pos", MPV_FORMAT_DOUBLE, &targetSeconds);
}

void MpvPlayer::onMpvEvents()
{
    while (m_mpv != nullptr) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) {
            break;
        }

        handleMpvEvent(event);
    }
}

void MpvPlayer::handleMpvEvent(mpv_event *event)
{
    switch (event->event_id) {
    case MPV_EVENT_PROPERTY_CHANGE: {
        auto *property = static_cast<mpv_event_property *>(event->data);
        if (property == nullptr || property->data == nullptr) {
            return;
        }

        if (std::strcmp(property->name, "time-pos") == 0) {
            double segmentStartSeconds = 0;
            double segmentCurrentSeconds = *static_cast<double *>(property->data);
            if (mpv_get_property(m_mpv, "demuxer-start-time", MPV_FORMAT_DOUBLE, &segmentStartSeconds) < 0) {
                segmentStartSeconds = 0;
            }

            const int seconds = std::max(0, static_cast<int>(segmentStartSeconds + segmentCurrentSeconds));
            if (seconds != m_currentSeconds) {
                m_currentSeconds = seconds;
                emit playPositionChanged(m_currentSeconds);
            }
        } else if (std::strcmp(property->name, "duration") == 0) {
            emit durationChanged(std::max(0, static_cast<int>(*static_cast<double *>(property->data))));
        }
        break;
    }
    case MPV_EVENT_END_FILE: {
        auto *endFile = static_cast<mpv_event_end_file *>(event->data);
        if (endFile != nullptr && endFile->reason == MPV_END_FILE_REASON_EOF) {
            emit endOfPlaylist();
        }
        break;
    }
    case MPV_EVENT_SHUTDOWN:
        m_mpv = nullptr;
        break;
    default:
        break;
    }
}
