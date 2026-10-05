#include "login.h"
#include "datacenter.h"

#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUuid>
#include <QtTest>

// 操作真实登录控件并接收本地 HTTP 请求；凭证只留在内存，失败断言不展开凭证。
class LoginTest : public QObject
{
    Q_OBJECT
    QTcpServer server;
    QByteArray previousBaseUrl;
    bool hadBaseUrl = false;
    int loginRequestCount = 0;
    int warningCount = 0;
    bool payloadMatches = false;
    QString expectedAccount;
    QString expectedPassword;

    bool enterPasswordMode(Login &login)
    {
        login.show();
        for (auto *button : login.findChildren<QPushButton *>()) {
            if (button->text() == QStringLiteral("密码登录")) {
                QTest::mouseClick(button, Qt::LeftButton);
                return true;
            }
        }
        return false;
    }

    // 自动关闭本地校验弹窗，让修复前的拒绝也能转化为有限时间内的断言失败。
    void dismissWarnings()
    {
        for (auto *widget : QApplication::topLevelWidgets()) {
            auto *message = qobject_cast<QMessageBox *>(widget);
            if (message && message->isVisible()) {
                ++warningCount;
                message->accept();
            }
        }
    }

private slots:
    void initTestCase()
    {
        hadBaseUrl = qEnvironmentVariableIsSet("VIDEO_API_BASE_URL");
        previousBaseUrl = qgetenv("VIDEO_API_BASE_URL");
        QVERIFY(server.listen(QHostAddress::LocalHost));
        qputenv("VIDEO_API_BASE_URL",
            QString("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        connect(&server, &QTcpServer::newConnection, this, [this]() {
            while (server.hasPendingConnections()) {
                auto *socket = server.nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                    const QByteArray data = socket->property("buffer").toByteArray() + socket->readAll();
                    socket->setProperty("buffer", data);
                    if (socket->property("handled").toBool()) return;
                    const int split = data.indexOf("\r\n\r\n");
                    if (split < 0) return;
                    int length = 0;
                    for (const auto &line : data.left(split).split('\n')) {
                        if (line.toLower().startsWith("content-length:")) {
                            length = line.mid(15).trimmed().toInt();
                        }
                    }
                    if (data.size() < split + 4 + length) return;
                    socket->setProperty("handled", true);
                    const QJsonObject payload = QJsonDocument::fromJson(data.mid(split + 4, length)).object();
                    if (data.startsWith("POST /login HTTP/1.1\r\n")) ++loginRequestCount;
                    payloadMatches = payload["account"].toString() == expectedAccount
                        && payload["password"].toString() == expectedPassword;

                    const QJsonObject response{{"success", true}, {"userName", "test"},
                        {"account", expectedAccount},
                        {"token", QUuid::createUuid().toString(QUuid::WithoutBraces)}};
                    const QByteArray body = QJsonDocument(response).toJson(QJsonDocument::Compact);
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                        "Connection: close\r\nContent-Length: " + QByteArray::number(body.size())
                        + "\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
            }
        });
    }

    void init()
    {
        DataCenter::instance().clearCurrentUser();
        loginRequestCount = 0;
        warningCount = 0;
        payloadMatches = false;
        expectedAccount = QStringLiteral("test-account");
        expectedPassword.clear();
    }

    void existingCredentialsReachBackend_data()
    {
        QTest::addColumn<int>("scenario");
        QTest::newRow("numeric development credential") << 0;
        QTest::newRow("short existing password") << 1;
        QTest::newRow("long existing password") << 2;
        QTest::newRow("short existing account") << 3;
        QTest::newRow("long existing account") << 4;
    }

    void existingCredentialsReachBackend()
    {
        QFETCH(int, scenario);
        for (int digit = 1; digit <= 6; ++digit) expectedPassword += QChar('0' + digit);
        if (scenario == 1) expectedPassword = QString(1, QChar('x'));
        if (scenario == 2) expectedPassword = QString(24, QChar('x'));
        if (scenario == 3) expectedAccount = QStringLiteral("x");
        if (scenario == 4) expectedAccount = QString(40, QChar('x'));

        Login login;
        QVERIFY(enterPasswordMode(login));
        auto *account = login.findChild<QLineEdit *>("accountEdit");
        auto *password = login.findChild<QLineEdit *>("passwordEdit");
        auto *submit = login.findChild<QPushButton *>("loginBtn");
        QVERIFY(account && password && submit);
        QTest::keyClicks(account, expectedAccount);
        QTest::keyClicks(password, expectedPassword);
        QTimer warningCloser;
        connect(&warningCloser, &QTimer::timeout, this, &LoginTest::dismissWarnings);
        warningCloser.start(10);
        QTest::mouseClick(submit, Qt::LeftButton);

        QTRY_COMPARE_WITH_TIMEOUT(loginRequestCount, 1, 1000);
        QVERIFY(payloadMatches);
        QTRY_VERIFY(DataCenter::instance().isLoggedIn());
        QCOMPARE(warningCount, 0);
    }

    void emptyPasswordDoesNotSubmit()
    {
        Login login;
        QVERIFY(enterPasswordMode(login));
        auto *account = login.findChild<QLineEdit *>("accountEdit");
        auto *submit = login.findChild<QPushButton *>("loginBtn");
        QVERIFY(account && submit);
        QTest::keyClicks(account, expectedAccount);
        QTimer warningCloser;
        connect(&warningCloser, &QTimer::timeout, this, &LoginTest::dismissWarnings);
        warningCloser.start(10);
        QTest::mouseClick(submit, Qt::LeftButton);
        QTest::qWait(100);

        QCOMPARE(warningCount, 1);
        QCOMPARE(loginRequestCount, 0);
        QVERIFY(!DataCenter::instance().isLoggedIn());
    }

    void cleanupTestCase()
    {
        DataCenter::instance().clearCurrentUser();
        if (hadBaseUrl) qputenv("VIDEO_API_BASE_URL", previousBaseUrl);
        else qunsetenv("VIDEO_API_BASE_URL");
    }
};

QTEST_MAIN(LoginTest)
#include "login_test.moc"
