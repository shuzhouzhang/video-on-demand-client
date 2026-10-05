#include "apiclient.h"
#include "playerpage.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUuid>
#include <QtTest>

// 播放替身只记录命令，允许测试按指定顺序触发加载/进度事件，复现异步竞态。
MpvPlayer::MpvPlayer(QWidget *, QObject *parent) : QObject(parent) {}
MpvPlayer::~MpvPlayer() = default;
void MpvPlayer::startPlay(const QString &path)
{
    setProperty("path", path);
    setProperty("loads", property("loads").toInt() + 1);
}
void MpvPlayer::play() { setProperty("paused", false); }
void MpvPlayer::pause() { setProperty("paused", true); }
void MpvPlayer::setPlaySpeed(double) {}
void MpvPlayer::setVolume(int) {}
void MpvPlayer::setCurrentPlayPosition(int seconds) { setProperty("seek", seconds); }
void MpvPlayer::onMpvEvents() {}

class PlayerPageTest : public QObject
{
    Q_OBJECT
    QTcpServer server;
    QUrl origin;
    QList<QJsonObject> saved;
    int progressDelay = 0;
    bool failPlayUrl = false;
    bool progressSent = false;

    PlayerPage *newPage()
    {
        auto *page = new PlayerPage("video", "title", "author", "date", "03:00", "0", "0");
        page->setAttribute(Qt::WA_DeleteOnClose);
        page->show();
        return page;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(server.listen(QHostAddress::LocalHost));
        origin = QUrl(QString("http://127.0.0.1:%1").arg(server.serverPort()));
        qputenv("VIDEO_API_BASE_URL", origin.toEncoded());
        connect(&server, &QTcpServer::newConnection, this, [this]() {
            while (server.hasPendingConnections()) {
                auto *socket = server.nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                    const auto data = socket->property("buffer").toByteArray() + socket->readAll();
                    socket->setProperty("buffer", data);
                    if (socket->property("handled").toBool()) return;
                    const int split = data.indexOf("\r\n\r\n");
                    if (split < 0) return;
                    int length = 0;
                    for (const auto &line : data.left(split).split('\n')) {
                        if (line.toLower().startsWith("content-length:")) length = line.mid(15).trimmed().toInt();
                    }
                    if (data.size() < split + 4 + length) return;
                    socket->setProperty("handled", true);
                    const auto firstLine = data.left(data.indexOf('\r')).split(' ');
                    const QString path = QUrl(QString::fromUtf8(firstLine[1])).path();
                    QByteArray body = "{\"success\":true}";
                    int delay = 0;
                    bool progress = false;
                    if (path == "/videos/watch-progress" && firstLine[0] == "POST") {
                        saved.append(QJsonDocument::fromJson(data.mid(split + 4)).object());
                        delay = 80;
                    } else if (path == "/videos/watch-progress") {
                        body = "{\"success\":true,\"seconds\":42}";
                        delay = progressDelay;
                        progress = true;
                    } else if (path == "/videos/play-url") {
                        body = failPlayUrl ? "{\"success\":false}" : "{\"success\":true,\"playUrl\":\"/uploads/video.mp4\"}";
                    } else if (path == "/videos/detail") {
                        body = "{\"success\":true,\"video\":{\"id\":\"video\",\"title\":\"title\"}}";
                    } else if (path == "/videos/like-status") {
                        body = "{\"success\":true,\"liked\":false,\"likeCount\":\"0\"}";
                    }
                    QTimer::singleShot(delay, socket, [this, socket, body, progress]() {
                        socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "
                                      + QByteArray::number(body.size()) + "\r\n\r\n" + body);
                        socket->disconnectFromHost();
                        if (progress) progressSent = true;
                    });
                });
            }
        });
    }

    void init()
    {
        saved.clear(); progressDelay = 0; failPlayUrl = false; progressSent = false;
        DataCenter::instance().clearCurrentUser();
        QVERIFY(DataCenter::instance().saveSession("test", "account",
            QUuid::createUuid().toString(QUuid::WithoutBraces), origin));
    }

    void resumeWaitsForBothResponses_data()
    {
        QTest::addColumn<bool>("fileFirst");
        QTest::newRow("progress first") << false;
        QTest::newRow("file first") << true;
    }

    void resumeWaitsForBothResponses()
    {
        QFETCH(bool, fileFirst);
        progressDelay = fileFirst ? 300 : 0;
        auto *page = newPage();
        auto *mpv = page->findChild<MpvPlayer *>();
        QTRY_VERIFY(!mpv->property("path").toString().isEmpty());
        if (!fileFirst) { QTRY_VERIFY(progressSent); QTest::qWait(30); }
        QVERIFY(!mpv->property("seek").isValid());
        emit mpv->durationChanged(180);
        emit mpv->fileLoaded();
        QTRY_COMPARE(mpv->property("seek").toInt(), 42);
        delete page;
    }

    void closingUnloadedPageDoesNotOverwriteProgress()
    {
        auto *page = newPage();
        QTRY_VERIFY(progressSent);
        QPointer<PlayerPage> guard(page);
        page->close();
        QTRY_VERIFY(guard.isNull());
        QTest::qWait(100);
        QVERIFY(saved.isEmpty());
    }

    void finalSaveSurvivesPageDestruction()
    {
        auto *page = newPage();
        auto *mpv = page->findChild<MpvPlayer *>();
        QTRY_VERIFY(progressSent);
        QTRY_VERIFY(!mpv->property("path").toString().isEmpty());
        emit mpv->fileLoaded();
        emit mpv->playPositionChanged(73);
        QPointer<PlayerPage> guard(page);
        page->close();
        QTRY_VERIFY(guard.isNull());
        QTRY_COMPARE(saved.size(), 1);
        QCOMPARE(saved.first()["seconds"].toInt(), 73);
        QCOMPARE(saved.first()["account"].toString(), QString("account"));
        QTRY_VERIFY(QCoreApplication::instance()->findChildren<ApiClient *>().isEmpty());
    }

    void fallbackDoesNotOverwriteBackendProgress()
    {
        failPlayUrl = true;
        auto *page = newPage();
        auto *mpv = page->findChild<MpvPlayer *>();
        QTRY_VERIFY(mpv->property("path").toString().endsWith("test.mp4"));
        QTRY_VERIFY(progressSent);
        emit mpv->fileLoaded();
        emit mpv->playPositionChanged(10);
        QVERIFY(!mpv->property("seek").isValid());
        page->close();
        QTest::qWait(100);
        QVERIFY(saved.isEmpty());
    }

    void changedSessionDoesNotSavePreviousUsersProgress()
    {
        auto *page = newPage();
        auto *mpv = page->findChild<MpvPlayer *>();
        QTRY_VERIFY(progressSent);
        QTRY_VERIFY(!mpv->property("path").toString().isEmpty());
        emit mpv->fileLoaded();
        emit mpv->playPositionChanged(73);
        QVERIFY(DataCenter::instance().saveSession("other", "other-account",
            QUuid::createUuid().toString(QUuid::WithoutBraces), origin));
        page->close();
        QTest::qWait(100);
        QVERIFY(saved.isEmpty());
    }

    void playAfterEofReloadsFromStart()
    {
        auto *page = newPage();
        auto *mpv = page->findChild<MpvPlayer *>();
        QTRY_VERIFY(progressSent);
        QTRY_COMPARE(mpv->property("loads").toInt(), 1);
        emit mpv->durationChanged(180);
        emit mpv->fileLoaded();
        QTRY_COMPARE(mpv->property("seek").toInt(), 42);
        emit mpv->endOfPlaylist();
        QTRY_COMPARE(saved.size(), 1);
        QCOMPARE(saved.last()["seconds"].toInt(), 180);
        const QString media = mpv->property("path").toString();
        QTest::mouseClick(page->findChild<QPushButton *>("playBtn"), Qt::LeftButton);
        QCOMPARE(mpv->property("loads").toInt(), 2);
        QCOMPARE(mpv->property("path").toString(), media);
        QVERIFY(!mpv->property("paused").toBool());
        mpv->setProperty("seek", QVariant());
        emit mpv->fileLoaded();
        QVERIFY(!mpv->property("seek").isValid());
        emit mpv->playPositionChanged(2);
        page->close();
        QTRY_COMPARE(saved.size(), 2);
        QCOMPARE(saved.last()["seconds"].toInt(), 2);
    }
};

QTEST_MAIN(PlayerPageTest)
#include "playerpage_test.moc"
