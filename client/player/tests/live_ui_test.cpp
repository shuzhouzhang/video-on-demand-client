#include "apiclient.h"
#include "login.h"
#include "playerpage.h"

#include <QApplication>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTimer>
#include <QtTest>

// 手动运行的真实界面联调；仅通过 Qt 控件事件操作，不替换页面、HTTP 或 libmpv。
// 开发账号和 Token 只留在内存，结束时恢复原观看秒数并注销。
class LiveUiTest : public QObject
{
    Q_OBJECT
    ApiClient *api = nullptr;
    QPointer<PlayerPage> page;
    QString videoId;
    int originalSeconds = 0;
    int currentSeconds = -1;
    int durationSeconds = 0;
    bool restoreNeeded = false;
    bool unexpectedDialog = false;
    QTimer dialogGuard;

    bool waitFor(QSignalSpy &signal, int timeout = 15000)
    { return !signal.isEmpty() || signal.wait(timeout); }

    int readProgress()
    {
        QSignalSpy loaded(api, &ApiClient::watchProgressLoaded);
        api->fetchWatchProgress(videoId);
        return waitFor(loaded) ? loaded.first()[0].toInt() : -1;
    }

    bool saveProgress(int seconds)
    {
        QSignalSpy saved(api, &ApiClient::watchProgressSaved);
        api->saveWatchProgress(videoId, seconds);
        return waitFor(saved);
    }

    // 构造真实播放页，在异步网络回调到达前订阅真实播放器状态。
    MpvPlayer *openPage()
    {
        currentSeconds = -1;
        durationSeconds = 0;
        page = new PlayerPage(videoId, "Qt live UI acceptance", "", "", "00:00", "0", "0");
        page->setAttribute(Qt::WA_DeleteOnClose);
        auto *mpv = page->findChild<MpvPlayer *>();
        connect(mpv, &MpvPlayer::playPositionChanged, this, [this](int value) { currentSeconds = value; });
        connect(mpv, &MpvPlayer::durationChanged, this, [this](int value) { durationSeconds = value; });
        page->show();
        return mpv;
    }

    // 从样式提供的滑块把手位置开始拖动，避免点击轨道只触发单步翻页。
    int dragSlider(double fraction)
    {
        auto *slider = page->findChild<QSlider *>("videoSlider");
        QStyleOptionSlider option;
        option.initFrom(slider);
        option.orientation = slider->orientation();
        option.minimum = slider->minimum();
        option.maximum = slider->maximum();
        option.sliderPosition = option.sliderValue = slider->value();
        const QPoint from = slider->style()->subControlRect(QStyle::CC_Slider, &option,
            QStyle::SC_SliderHandle, slider).center();
        option.sliderPosition = option.sliderValue = qRound(fraction * slider->maximum());
        const QPoint to = slider->style()->subControlRect(QStyle::CC_Slider, &option,
            QStyle::SC_SliderHandle, slider).center();
        QTest::mousePress(slider, Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(slider, to, 40);
        // 松手后页面会按整秒回写滑块，必须在此之前记录用户选择的位置。
        const int targetSeconds = slider->value() * durationSeconds / slider->maximum();
        QTest::mouseRelease(slider, Qt::LeftButton, Qt::NoModifier, to);
        return targetSeconds;
    }

private slots:
    void initTestCase()
    {
        QVERIFY2(!qEnvironmentVariable("VIDEO_API_BASE_URL").isEmpty(), "Set VIDEO_API_BASE_URL explicitly");
        QVERIFY2(!qEnvironmentVariable("VOD_TEST_ACCOUNT").isEmpty(), "Set a development VOD_TEST_ACCOUNT");
        QVERIFY2(!qEnvironmentVariable("VOD_TEST_PASSWORD").isEmpty(), "Set VOD_TEST_PASSWORD in memory");
        videoId = qEnvironmentVariable("VOD_TEST_VIDEO_ID");
        QVERIFY2(!videoId.isEmpty(), "Set an existing VOD_TEST_VIDEO_ID explicitly");
        QApplication::setQuitOnLastWindowClosed(false);
        api = new ApiClient(this);
        // 登录失败的模态提示也必须能退出测试；不把提示内容或凭证写进报告。
        connect(&dialogGuard, &QTimer::timeout, this, [this]() {
            for (QWidget *widget : QApplication::topLevelWidgets()) {
                if (auto *dialog = qobject_cast<QMessageBox *>(widget); dialog && dialog->isVisible()) {
                    unexpectedDialog = true;
                    dialog->reject();
                }
            }
        });
        dialogGuard.start(50);
    }

    void loginPlaybackSeekResumeAndReplay()
    {
        Login login;
        QSignalSpy loggedIn(&login, &Login::loginSuccess);
        login.show();
        QPushButton *passwordMode = nullptr;
        for (auto *button : login.findChildren<QPushButton *>())
            if (button->text() == QStringLiteral("密码登录")) passwordMode = button;
        QVERIFY(passwordMode);
        QTest::mouseClick(passwordMode, Qt::LeftButton);
        auto *account = login.findChild<QLineEdit *>("accountEdit");
        auto *password = login.findChild<QLineEdit *>("passwordEdit");
        auto *loginButton = login.findChild<QPushButton *>("loginBtn");
        QVERIFY(account && password && loginButton);
        QTest::keyClicks(account, qEnvironmentVariable("VOD_TEST_ACCOUNT"));
        QTest::keyClicks(password, qEnvironmentVariable("VOD_TEST_PASSWORD"));
        QTest::mouseClick(loginButton, Qt::LeftButton);
        QVERIFY2(waitFor(loggedIn), "The real Login form did not complete login");
        QVERIFY(!unexpectedDialog);
        QVERIFY(DataCenter::instance().isLoggedIn());
        qInfo("PASS: real password-mode form login");

        originalSeconds = readProgress();
        QVERIFY(originalSeconds >= 0);
        restoreNeeded = true;
        QVERIFY(saveProgress(0));

        auto *mpv = openPage();
        QVERIFY(mpv);
        QSignalSpy loaded(mpv, &MpvPlayer::fileLoaded);
        auto *pageApi = page->findChild<ApiClient *>(QString(), Qt::FindDirectChildrenOnly);
        QVERIFY(pageApi);
        QSignalSpy playUrl(pageApi, &ApiClient::playUrlLoaded);
        QVERIFY2(waitFor(playUrl), "Backend play URL failed; a fallback cannot pass this test");
        QVERIFY2(waitFor(loaded, 20000), "Real libmpv did not load the page media");
        QTRY_VERIFY_WITH_TIMEOUT(durationSeconds >= 4 && currentSeconds == 0, 15000);
        auto *play = page->findChild<QPushButton *>("playBtn");
        QVERIFY(play);
        QTest::mouseClick(play, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(currentSeconds >= 1, 6000);
        QTest::mouseClick(play, Qt::LeftButton);
        QTest::qWait(250);
        const int pausedAt = currentSeconds;
        QTest::qWait(1200);
        QCOMPARE(currentSeconds, pausedAt);
        qInfo("PASS: real page load, play and pause buttons");

        const int seekSeconds = dragSlider(0.55);
        QVERIFY2(seekSeconds > pausedAt, "Slider drag did not advance beyond the paused position");
        QTRY_COMPARE_WITH_TIMEOUT(currentSeconds, seekSeconds, 5000);
        auto *close = page->findChild<QPushButton *>("quitBtn");
        QVERIFY(close);
        QTest::mouseClick(close, Qt::LeftButton);
        QTRY_VERIFY(page.isNull());
        // 等待应用持有的最后一次保存完成，再读取，避免读取抢在关闭保存之前。
        QTRY_VERIFY_WITH_TIMEOUT(QCoreApplication::instance()->findChildren<ApiClient *>(
            QString(), Qt::FindDirectChildrenOnly).isEmpty(), 15000);
        QCOMPARE(readProgress(), seekSeconds);
        qInfo("PASS: slider drag and final progress persisted after page destruction");

        mpv = openPage();
        QSignalSpy reopened(mpv, &MpvPlayer::fileLoaded);
        QVERIFY(waitFor(reopened, 20000));
        QTRY_COMPARE_WITH_TIMEOUT(currentSeconds, seekSeconds, 15000);
        QVERIFY(durationSeconds > seekSeconds);
        qInfo("PASS: reopening the real page resumes at the saved position");

        QSignalSpy ended(mpv, &MpvPlayer::endOfPlaylist);
        const int nearEnd = dragSlider((durationSeconds - 1.0) / durationSeconds);
        QTRY_COMPARE_WITH_TIMEOUT(currentSeconds, nearEnd, 5000);
        play = page->findChild<QPushButton *>("playBtn");
        QTest::mouseClick(play, Qt::LeftButton);
        QVERIFY2(waitFor(ended, 10000), "Playback did not reach EOF");
        QSignalSpy replayPositions(mpv, &MpvPlayer::playPositionChanged);
        QTest::mouseClick(play, Qt::LeftButton);
        QTRY_COMPARE_WITH_TIMEOUT(reopened.count(), 2, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(currentSeconds >= 1 && currentSeconds < durationSeconds - 1, 6000);
        QVERIFY(!replayPositions.isEmpty());
        QCOMPARE(replayPositions.first()[0].toInt(), 0);
        QVERIFY(!unexpectedDialog);
        qInfo("PASS: EOF play button reloads media and starts playback from zero");
    }

    void cleanupTestCase()
    {
        if (!api) return;
        if (page) page->close();
        const bool closed = QTest::qWaitFor([this]() { return page.isNull(); }, 10000);
        const bool drained = QTest::qWaitFor([]() {
            return QCoreApplication::instance()->findChildren<ApiClient *>(
                QString(), Qt::FindDirectChildrenOnly).isEmpty();
        }, 15000);
        const bool restored = closed && drained && (!restoreNeeded
            || (saveProgress(originalSeconds) && readProgress() == originalSeconds));
        bool loggedOut = true;
        if (DataCenter::instance().isLoggedIn()) {
            QSignalSpy done(api, &ApiClient::logoutSucceeded);
            api->logout();
            loggedOut = waitFor(done);
        }
        dialogGuard.stop();
        QVERIFY2(restored, "Cleanup could not restore the original watch progress");
        QVERIFY2(loggedOut, "Cleanup could not revoke the test session");
        qInfo("PASS: original progress restored and test session revoked");
    }
};

QTEST_MAIN(LiveUiTest)
#include "live_ui_test.moc"
