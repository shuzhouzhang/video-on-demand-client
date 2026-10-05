#include "apiclient.h"
#include "mpv/mpvplayer.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSignalSpy>
#include <QUrlQuery>
#include <QtTest>

// 手动联调真实后端；账号密码来自进程环境，Token 只留在内存且不进入断言输出。
// 测试临时改写指定视频的观看秒数，退出时恢复原值并注销会话。
class LiveBackendTest : public QObject
{
    Q_OBJECT
    ApiClient *api = nullptr;
    QString videoId;
    int originalSeconds = 0;
    bool restoreNeeded = false;

    bool waitFor(QSignalSpy &signal, int timeout = 15000)
    { return !signal.isEmpty() || signal.wait(timeout); }

    bool restoreProgress()
    {
        if (!restoreNeeded) return true;
        QSignalSpy saved(api, &ApiClient::watchProgressSaved);
        api->saveWatchProgress(videoId, originalSeconds);
        if (!waitFor(saved)) return false;
        restoreNeeded = false;
        return true;
    }

private slots:
    void initTestCase()
    {
        QVERIFY2(!qEnvironmentVariable("VIDEO_API_BASE_URL").isEmpty(), "Set VIDEO_API_BASE_URL explicitly");
        QVERIFY2(!qEnvironmentVariable("VOD_TEST_ACCOUNT").isEmpty(), "Set VOD_TEST_ACCOUNT to a development account");
        QVERIFY2(!qEnvironmentVariable("VOD_TEST_PASSWORD").isEmpty(), "Set VOD_TEST_PASSWORD in the process environment");
        api = new ApiClient(this);
    }

    void loginPlaybackProgressAndLogout()
    {
        QSignalSpy loggedIn(api, &ApiClient::loginSucceeded);
        api->login(qEnvironmentVariable("VOD_TEST_ACCOUNT"), qEnvironmentVariable("VOD_TEST_PASSWORD"));
        QVERIFY2(waitFor(loggedIn), "Live login failed");
        QVERIFY(DataCenter::instance().isLoggedIn());
        qInfo("PASS: real ApiClient login");

        // 第二个实例验证共享会话确实进入真实受保护请求。
        ApiClient shared;
        QSignalSpy profile(&shared, &ApiClient::userProfileLoaded);
        shared.fetchUserProfile();
        QVERIFY2(waitFor(profile), "Shared session could not fetch the profile");
        qInfo("PASS: shared session and protected profile request");

        QSignalSpy videos(api, &ApiClient::videosLoaded);
        api->fetchVideos();
        QVERIFY2(waitFor(videos), "Live video list failed");
        const auto items = qvariant_cast<QList<VideoInfo>>(videos.first()[0]);
        QVERIFY2(!items.isEmpty(), "The backend has no public video to test");
        videoId = qEnvironmentVariable("VOD_TEST_VIDEO_ID");
        if (videoId.isEmpty()) videoId = items.first().id;
        QSignalSpy detail(api, &ApiClient::videoDetailLoaded);
        api->fetchVideoDetail(videoId);
        QVERIFY2(waitFor(detail), "Video detail failed");
        qInfo("PASS: video list and detail");

        QSignalSpy playUrl(api, &ApiClient::playUrlLoaded);
        api->fetchPlayUrl(videoId);
        QVERIFY2(waitFor(playUrl), "Play URL failed");
        const QUrl mediaUrl(playUrl.first()[0].toString());
        QVERIFY2(mediaUrl.scheme() == "http" || mediaUrl.scheme() == "https", "The backend must supply an HTTP media resource");
        QNetworkAccessManager network;
        QNetworkRequest request(mediaUrl);
        request.setTransferTimeout(15000);
        request.setRawHeader("Range", "bytes=0-1023");
        auto *media = network.get(request);
        QSignalSpy downloaded(media, &QNetworkReply::finished);
        QVERIFY2(waitFor(downloaded), "Media request timed out");
        QVERIFY2(media->error() == QNetworkReply::NoError && !media->readAll().isEmpty(), "Media resource is not accessible");
        media->deleteLater();
        qInfo("PASS: actual HTTP media resource");

        MpvPlayer player;
        QSignalSpy loaded(&player, &MpvPlayer::fileLoaded);
        QSignalSpy duration(&player, &MpvPlayer::durationChanged);
        QSignalSpy positions(&player, &MpvPlayer::playPositionChanged);
        player.startPlay(mediaUrl.toString());
        player.pause();
        QVERIFY2(waitFor(loaded, 20000), "Real libmpv could not load the HTTP media");
        QVERIFY2(waitFor(duration), "Media duration is unavailable");
        const int length = duration.last()[0].toInt();
        QVERIFY2(length >= 2, "Use a video at least two seconds long for seek verification");
        const int target = qMin(3, length - 1);
        positions.clear();
        player.setCurrentPlayPosition(target);
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(!positions.isEmpty() && positions.last()[0].toInt() >= target, 15000);
        player.pause();
        qInfo("PASS: real libmpv file-loaded, seek and playback progress");

        QSignalSpy progress(api, &ApiClient::watchProgressLoaded);
        api->fetchWatchProgress(videoId);
        QVERIFY2(waitFor(progress), "Original watch progress could not be read");
        originalSeconds = progress.first()[0].toInt();
        const int testSeconds = originalSeconds == target ? target + 1 : target;
        restoreNeeded = true;
        QSignalSpy saved(api, &ApiClient::watchProgressSaved);
        api->saveWatchProgress(videoId, testSeconds);
        QVERIFY2(waitFor(saved), "Watch progress could not be saved");
        progress.clear();
        api->fetchWatchProgress(videoId);
        QVERIFY(waitFor(progress));
        QCOMPARE(progress.first()[0].toInt(), testSeconds);
        QVERIFY2(restoreProgress(), "Original watch progress could not be restored");
        progress.clear();
        api->fetchWatchProgress(videoId);
        QVERIFY(waitFor(progress));
        QCOMPARE(progress.first()[0].toInt(), originalSeconds);
        qInfo("PASS: watch progress write/read and original value restored");

        const QUrl base(qEnvironmentVariable("VIDEO_API_BASE_URL"));
        const QString oldToken = DataCenter::instance().tokenFor(base);
        const QString account = DataCenter::instance().currentUser().account;
        QSignalSpy loggedOut(api, &ApiClient::logoutSucceeded);
        api->logout();
        QVERIFY(!DataCenter::instance().isLoggedIn());
        QVERIFY2(waitFor(loggedOut), "Server logout failed");
        QUrl profileUrl = base.resolved(QUrl("/users/profile"));
        QUrlQuery query;
        query.addQueryItem("account", account);
        profileUrl.setQuery(query);
        QNetworkRequest staleRequest(profileUrl);
        staleRequest.setTransferTimeout(10000);
        staleRequest.setRawHeader("Authorization", "Bearer " + oldToken.toUtf8());
        auto *stale = network.get(staleRequest);
        QSignalSpy rejected(stale, &QNetworkReply::finished);
        QVERIFY(waitFor(rejected));
        QCOMPARE(stale->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 401);
        stale->deleteLater();
        qInfo("PASS: logout revokes the old credential");
    }

    void cleanupTestCase()
    {
        if (!api) return;
        const bool restored = restoreProgress();
        bool loggedOut = true;
        if (DataCenter::instance().isLoggedIn()) {
            QSignalSpy done(api, &ApiClient::logoutSucceeded);
            api->logout();
            loggedOut = waitFor(done);
        }
        QVERIFY2(restored, "Cleanup could not restore watch progress");
        QVERIFY2(loggedOut, "Cleanup could not revoke the test session");
    }
};

QTEST_GUILESS_MAIN(LiveBackendTest)
#include "live_backend_test.moc"
