#include "mpv/mpvplayer.h"

#include <QSignalSpy>
#include <QtTest>
#include <deque>
#include <cstring>

// 在真实 MpvPlayer 外包一层 C API 替身，用可控事件检查时间戳和句柄释放。
struct mpv_handle {};
namespace {
std::deque<mpv_event> events;
mpv_event currentEvent{};
void (*wakeupCallback)(void *) = nullptr;
void *wakeupContext = nullptr;
int destroyCount = 0;
}

extern "C" {
mpv_handle *mpv_create() { return new mpv_handle; }
int mpv_initialize(mpv_handle *) { return 0; }
int mpv_set_option(mpv_handle *, const char *, mpv_format, void *) { return 0; }
int mpv_set_option_string(mpv_handle *, const char *, const char *) { return 0; }
int mpv_observe_property(mpv_handle *, uint64_t, const char *, mpv_format) { return 0; }
int mpv_command_async(mpv_handle *, uint64_t, const char **) { return 0; }
int mpv_set_property_async(mpv_handle *, uint64_t, const char *, mpv_format, void *) { return 0; }
int mpv_get_property(mpv_handle *, const char *name, mpv_format, void *data)
{
    if (std::strcmp(name, "demuxer-start-time") != 0) return -1;
    *static_cast<double *>(data) = 90;
    return 0;
}
void mpv_set_wakeup_callback(mpv_handle *, void (*callback)(void *), void *context)
{ wakeupCallback = callback; wakeupContext = context; }
void mpv_destroy(mpv_handle *handle) { ++destroyCount; delete handle; }
void mpv_terminate_destroy(mpv_handle *handle) { mpv_destroy(handle); }
mpv_event *mpv_wait_event(mpv_handle *, double)
{
    currentEvent = {};
    if (!events.empty()) { currentEvent = events.front(); events.pop_front(); }
    return &currentEvent;
}
}

class MpvPlayerTest : public QObject
{
    Q_OBJECT
private slots:
    void init() { events.clear(); destroyCount = 0; }

    void positionDoesNotAddDemuxerTimestamp()
    {
        MpvPlayer player;
        QSignalSpy position(&player, &MpvPlayer::playPositionChanged);
        double seconds = 3.5;
        mpv_event_property property{"time-pos", MPV_FORMAT_DOUBLE, &seconds};
        events.push_back({MPV_EVENT_PROPERTY_CHANGE, 0, 0, &property});
        wakeupCallback(wakeupContext);
        QTRY_COMPARE(position.count(), 1);
        QCOMPARE(position.first()[0].toInt(), 3);
    }

    void loadedSignalRequiresLoadedEvent()
    {
        MpvPlayer player;
        QSignalSpy loaded(&player, &MpvPlayer::fileLoaded);
        player.startPlay("video.mp4");
        QCOMPARE(loaded.count(), 0);
        events.push_back({MPV_EVENT_FILE_LOADED, 0, 0, nullptr});
        wakeupCallback(wakeupContext);
        QTRY_COMPARE(loaded.count(), 1);
    }

    void shutdownReleasesHandleExactlyOnce()
    {
        {
            MpvPlayer player;
            events.push_back({MPV_EVENT_SHUTDOWN, 0, 0, nullptr});
            wakeupCallback(wakeupContext);
            QTRY_COMPARE(destroyCount, 1);
            QVERIFY(wakeupCallback == nullptr);
        }
        QCOMPARE(destroyCount, 1);
    }
};

QTEST_GUILESS_MAIN(MpvPlayerTest)
#include "mpvplayer_test.moc"
