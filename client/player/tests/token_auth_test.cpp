#include "apiclient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <QTimer>
#include <QUuid>
#include <QtTest>
#include <functional>

// 本地 HTTP 替身接收真实 Qt 请求；凭证随机生成且断言只输出布尔值。
class TokenAuthTest : public QObject
{
    Q_OBJECT
    struct Response { int status; QByteArray body; int delay; QByteArray headers; };
    QTcpServer server;
    QList<Response> responses;
    QList<QByteArray> requests;
    QUrl origin;
    QString token;

    void enqueue(int status = 200, QByteArray body = "{\"success\":true}",
                 int delay = 0, QByteArray headers = {})
    { responses.append({status, body, delay, headers}); }

    QByteArray loginBody(bool withToken = true)
    {
        QJsonObject obj{{"success", true}, {"userName", "test"}, {"account", "test-account"}};
        if (withToken) obj["token"] = token;
        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    bool hasToken(const QByteArray &request) const
    { return request.contains("Authorization: Bearer " + token.toUtf8() + "\r\n"); }

    void establish()
    {
        token = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVERIFY(DataCenter::instance().saveSession("test", "test-account", token, origin));
    }

private slots:
    void emptyVideoListClearsCache()
    {
        DataCenter::instance().setHomeVideos({VideoInfo{"old", "old title"}});
        ApiClient api;
        QSignalSpy loaded(&api, &ApiClient::videosLoaded), failed(&api, &ApiClient::requestFailed);
        enqueue(200, "[]"); api.fetchVideos();
        QTRY_COMPARE(loaded.count(), 1);
        QCOMPARE(failed.count(), 0);
        const auto videos = qvariant_cast<QList<VideoInfo>>(loaded.first()[0]);
        QVERIFY(videos.isEmpty());
        DataCenter::instance().setHomeVideos(videos);
        QVERIFY(DataCenter::instance().homeVideos().isEmpty());
    }

    void malformedVideoListIsRejected_data()
    {
        QTest::addColumn<QByteArray>("body");
        QTest::newRow("broken JSON") << QByteArray("[");
        QTest::newRow("object") << QByteArray("{\"success\":false}");
        QTest::newRow("null") << QByteArray("null");
    }

    void malformedVideoListIsRejected()
    {
        QFETCH(QByteArray, body);
        ApiClient api;
        QSignalSpy loaded(&api, &ApiClient::videosLoaded), failed(&api, &ApiClient::requestFailed);
        enqueue(200, body); api.fetchVideos();
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(loaded.count(), 0);
    }

    void initTestCase()
    {
        QVERIFY(server.listen(QHostAddress::LocalHost));
        origin = QUrl(QString("http://127.0.0.1:%1").arg(server.serverPort()));
        qputenv("VIDEO_API_BASE_URL", origin.toEncoded());
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (server.hasPendingConnections()) {
                auto *socket = server.nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    auto data = socket->property("buffer").toByteArray() + socket->readAll();
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
                    requests.append(data);
                    if (responses.isEmpty()) { socket->disconnectFromHost(); return; }
                    const auto response = responses.takeFirst();
                    QTimer::singleShot(response.delay, socket, [socket, response] {
                        socket->write("HTTP/1.1 " + QByteArray::number(response.status)
                            + " Test\r\nContent-Type: application/json\r\nConnection: close\r\n"
                            + response.headers + "Content-Length: " + QByteArray::number(response.body.size())
                            + "\r\n\r\n" + response.body);
                        socket->disconnectFromHost();
                    });
                });
            }
        });
    }

    void init()
    {
        DataCenter::instance().clearCurrentUser();
        requests.clear(); responses.clear();
        token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    void passwordAndEmailShareSession()
    {
        ApiClient loginClient, businessClient;
        QSignalSpy loggedIn(&loginClient, &ApiClient::loginSucceeded);
        QSignalSpy favorites(&businessClient, &ApiClient::favoriteVideosLoaded);
        enqueue(200, loginBody());
        loginClient.login("test-account", "test-password");
        QTRY_COMPARE(loggedIn.count(), 1);
        QVERIFY(DataCenter::instance().isLoggedIn());
        QVERIFY(!requests[0].contains("Authorization:"));
        enqueue(200, "{\"success\":true,\"videos\":[]}");
        businessClient.fetchFavoriteVideos();
        QTRY_COMPARE(favorites.count(), 1);
        QVERIFY(hasToken(requests.last()));
        DataCenter::instance().clearCurrentUser();
        enqueue(200, loginBody());
        loginClient.emailLogin("test@example.invalid", "code-session", "123456");
        QTRY_COMPARE(loggedIn.count(), 2);
        QVERIFY(DataCenter::instance().tokenFor(origin) == token);
        QVERIFY(!requests.last().contains("Authorization:"));
    }

    void missingOrInvalidToken()
    {
        ApiClient api;
        QSignalSpy failed(&api, &ApiClient::loginFailed);
        enqueue(200, loginBody(false)); api.login("test-account", "test-password");
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(!DataCenter::instance().isLoggedIn());
        enqueue(200, loginBody(false)); api.emailLogin("test@example.invalid", "session", "123456");
        QTRY_COMPARE(failed.count(), 2);
        QVERIFY(!DataCenter::instance().isLoggedIn());
        DataCenter::instance().setCurrentUser("test", "test-account");
        QVERIFY(!DataCenter::instance().isLoggedIn());
        QVERIFY(!DataCenter::instance().saveSession("test", "test-account", "bad\r\nheader", origin));
        QVERIFY(!DataCenter::instance().saveSession("test", "test-account", "bad\n", origin));
    }

    void allBusinessPathsCarryToken()
    {
        establish(); ApiClient api;
        QTemporaryFile file; QVERIFY(file.open()); file.write("test-file"); file.flush();
        UploadVideoInfo upload;
        upload.account = "test-account"; upload.userName = "test"; upload.title = "test";
        upload.videoFilePath = file.fileName(); upload.videoFileName = "test.mp4";
        const QList<std::function<void()>> calls = {
            [&]{api.fetchVideos();}, [&]{api.fetchPlayUrl("video");},
            [&]{api.fetchBarrages("video");}, [&]{api.sendBarrage("video", 1, "test");},
            [&]{api.fetchVideoDetail("video");}, [&]{api.likeVideo("video");},
            [&]{api.unlikeVideo("video");}, [&]{api.fetchVideoLikeStatus("video");},
            [&]{api.fetchWatchProgress("video");}, [&]{api.saveWatchProgress("video", 1);},
            [&]{api.fetchComments("video");}, [&]{api.sendComment("video", "test");},
            [&]{api.searchVideos("test");}, [&]{api.fetchFavoriteStatus("video");},
            [&]{api.favoriteVideo("video");}, [&]{api.unfavoriteVideo("video");},
            [&]{api.fetchFavoriteVideos();}, [&]{api.fetchUserProfile();},
            [&]{api.updateUserProfile("test", "description");}, [&]{api.fetchMyVideos();},
            [&]{api.uploadAvatar(file.fileName());}, [&]{api.uploadVideo(upload);},
            [&]{api.fetchAdminReviews();}, [&]{api.reviewVideo("video", "approved");},
            [&]{api.fetchAdminUsers();}, [&]{api.updateAdminUser("test-account", "disable");}
        };
        for (const auto &call : calls) {
            const int count = requests.size(); enqueue(403); call();
            QTRY_COMPARE(requests.size(), count + 1);
            QVERIFY(hasToken(requests.last()));
        }
        QVERIFY(DataCenter::instance().isLoggedIn());
        QVERIFY(DataCenter::instance().tokenFor(QUrl("https://media.example.invalid/video")).isEmpty());
        QUrl otherPort = origin; otherPort.setPort(origin.port() + 1);
        QVERIFY(DataCenter::instance().tokenFor(otherPort).isEmpty());
    }

    void forbiddenAndUnavailableKeepSession()
    {
        establish(); ApiClient api;
        QSignalSpy failed(&api, &ApiClient::favoriteRequestFailed);
        QSignalSpy cleared(&DataCenter::instance(), &DataCenter::sessionCleared);
        enqueue(403); api.favoriteVideo("video"); QTRY_COMPARE(failed.count(), 1);
        QVERIFY(failed.last()[0].toString().contains("403"));
        enqueue(503); api.favoriteVideo("video"); QTRY_COMPARE(failed.count(), 2);
        QVERIFY(failed.last()[0].toString().contains("503"));
        QVERIFY(DataCenter::instance().isLoggedIn()); QCOMPARE(cleared.count(), 0);
        QCOMPARE(requests.size(), 2); // 写请求没有自动重试。
    }

    void loginRejectionDoesNotExpireExistingSession()
    {
        establish(); ApiClient api;
        QSignalSpy failed(&api, &ApiClient::loginFailed);
        enqueue(401); api.login("test-account", "wrong-password");
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(!failed.first()[0].toString().isEmpty());
        QVERIFY(DataCenter::instance().isLoggedIn());
        QVERIFY(!requests.first().contains("Authorization:"));
    }

    void differentOriginDoesNotReceiveCredential()
    {
        establish();
        QUrl other = origin; other.setHost("localhost");
        qputenv("VIDEO_API_BASE_URL", other.toEncoded());
        ApiClient api;
        qputenv("VIDEO_API_BASE_URL", origin.toEncoded());
        QSignalSpy failed(&api, &ApiClient::adminRequestFailed);
        enqueue(403); api.fetchAdminUsers();
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(!requests.first().contains("Authorization:"));
        QVERIFY(DataCenter::instance().isLoggedIn());
    }

    void concurrentUnauthorizedClearsOnce()
    {
        establish(); ApiClient a, b;
        QSignalSpy cleared(&DataCenter::instance(), &DataCenter::sessionCleared);
        QSignalSpy failedA(&a, &ApiClient::favoriteRequestFailed), failedB(&b, &ApiClient::favoriteRequestFailed);
        enqueue(401, "{}", 80); enqueue(401, "{}", 80);
        a.favoriteVideo("video"); b.fetchFavoriteVideos();
        QTRY_COMPARE(failedA.count(), 1); QTRY_COMPARE(failedB.count(), 1);
        QCOMPARE(cleared.count(), 1); QVERIFY(cleared.first()[0].toBool());
        QVERIFY(!DataCenter::instance().isLoggedIn()); QVERIFY(DataCenter::instance().tokenFor(origin).isEmpty());
        QVERIFY(failedA.first()[0].toString().isEmpty()); QVERIFY(failedB.first()[0].toString().isEmpty());
        QCOMPARE(requests.size(), 2);
    }

    void oldResponsesCannotChangeNewSession()
    {
        establish(); ApiClient api;
        QSignalSpy failed(&api, &ApiClient::favoriteRequestFailed);
        enqueue(401, "{}", 150); api.favoriteVideo("video"); QTRY_COMPARE(requests.size(), 1);
        establish(); QTRY_COMPARE(failed.count(), 1);
        QVERIFY(DataCenter::instance().isLoggedIn());
        QSignalSpy profileFailed(&api, &ApiClient::userProfileFailed), loaded(&api, &ApiClient::userProfileLoaded);
        enqueue(200, "{\"success\":true,\"account\":\"test-account\",\"userName\":\"old\"}", 150);
        api.fetchUserProfile(); QTRY_COMPARE(requests.size(), 2);
        DataCenter::instance().clearCurrentUser();
        QTRY_COMPARE(profileFailed.count(), 1); QCOMPARE(loaded.count(), 0);
        QVERIFY(!DataCenter::instance().isLoggedIn());
    }

    void logoutClearsEvenOnFailure()
    {
        establish(); ApiClient api;
        QSignalSpy failed(&api, &ApiClient::logoutFailed);
        enqueue(503); api.logout();
        QVERIFY(!DataCenter::instance().isLoggedIn());
        QTRY_COMPARE(failed.count(), 1); QVERIFY(hasToken(requests.first()));
        QVERIFY(DataCenter::instance().tokenFor(origin).isEmpty());
        establish(); QSignalSpy succeeded(&api, &ApiClient::logoutSucceeded);
        enqueue(); api.logout(); QTRY_COMPARE(succeeded.count(), 1);
        QVERIFY(!DataCenter::instance().isLoggedIn());
        QSignalSpy adminFailed(&api, &ApiClient::adminRequestFailed);
        enqueue(401); api.fetchAdminUsers(); QTRY_COMPARE(adminFailed.count(), 1);
        QVERIFY(!requests.last().contains("Authorization:"));
    }

    void lateLoginCannotUndoLogout()
    {
        ApiClient api;
        QSignalSpy failed(&api, &ApiClient::loginFailed), succeeded(&api, &ApiClient::loginSucceeded);
        enqueue(200, loginBody(), 150); api.login("test-account", "test-password");
        QTRY_COMPARE(requests.size(), 1);
        DataCenter::instance().clearCurrentUser();
        QTRY_COMPARE(failed.count(), 1); QCOMPARE(succeeded.count(), 0);
        QVERIFY(!DataCenter::instance().isLoggedIn());
    }

    void redirectDoesNotForwardOrReplay()
    {
        establish(); ApiClient api;
        QSignalSpy failed(&api, &ApiClient::favoriteRequestFailed);
        enqueue(307, "{}", 0, "Location: " + origin.toEncoded() + "/outside\r\n");
        api.favoriteVideo("video"); QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(requests.size(), 1); QVERIFY(DataCenter::instance().isLoggedIn());
    }
};

QTEST_GUILESS_MAIN(TokenAuthTest)
#include "token_auth_test.moc"
