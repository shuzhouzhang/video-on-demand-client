#include "apiclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

ApiClient::ApiClient(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
}

void ApiClient::fetchVideos()
{
    // 这是什么：描述一次 GET /videos 请求。
    // 为什么能实现：QNetworkRequest 保存 URL 和请求头，QNetworkAccessManager 根据它发出 HTTP 请求。
    // 什么时候调用：每次首页需要从后端重新拉取视频列表时执行。
    // 和谁配合：m_videosUrl 指向 mock server 或真实后端的视频列表接口。
    QNetworkRequest request(m_videosUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 这是什么：真正发起 GET 请求，并拿到代表本次响应的 reply。
    // 为什么能实现：Qt 网络请求是异步的，get() 立即返回 QNetworkReply，数据稍后通过 finished 信号到达。
    // 什么时候调用：request 准备好后立刻调用。
    // 和谁配合：下面的 finished 槽函数负责在后端响应后读取 reply。
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 这是什么：处理本次网络请求完成后的结果。
        // 为什么能实现：finished 在后端响应、网络失败或请求取消时触发，此时 reply 里已经有状态和响应体。
        // 什么时候调用：Qt 事件循环收到本次 HTTP 请求完成信号时自动调用。
        // 和谁配合：成功时交给 parseVideosFromJson，失败时通过 requestFailed 通知 player.cpp。
        if (reply->error() != QNetworkReply::NoError) {
            emit requestFailed(reply->errorString());
            reply->deleteLater();
            return;
        }

        // 这是什么：读取后端返回的 JSON 文本，并转换成首页能直接使用的视频结构。
        // 为什么能实现：readAll() 取出响应体，parseVideosFromJson() 按 VideoInfo 字段解析 JSON 数组。
        // 什么时候调用：确认没有网络错误后调用。
        // 和谁配合：parseVideosFromJson 在 DataCenter 模块里，保持“JSON 翻译”逻辑复用。
        const QByteArray data = reply->readAll();
        const QList<VideoInfo> videos = parseVideosFromJson(data);
        if (videos.isEmpty()) {
            emit requestFailed("视频列表为空或 JSON 格式不正确");
            reply->deleteLater();
            return;
        }

        // 这是什么：把接口得到的视频列表发给页面。
        // 为什么能实现：videosLoaded 是 Qt 信号，player.cpp 已经 connect 到 setHomeVideos()。
        // 什么时候调用：网络成功且 JSON 至少解析出一个视频后调用。
        // 和谁配合：player.cpp 更新 m_homeVideos 并重新 renderHomeVideos()。
        emit videosLoaded(videos);
        reply->deleteLater();
    });
}

void ApiClient::login(const QString &account, const QString &password)
{
    // 这是什么：描述一次 POST /login 请求。
    // 为什么能实现：登录接口需要把账号密码作为 JSON 请求体发给后端，QNetworkRequest 保存 URL 和 Content-Type。
    // 什么时候调用：Login 的密码登录表单通过本地基础校验后调用。
    // 和谁配合：m_loginUrl 指向 mock server 或真实后端的登录接口。
    QNetworkRequest request(m_loginUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject payload;
    payload["account"] = account;
    payload["password"] = password;
    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    // 这是什么：真正发起登录 POST 请求，并拿到本次响应对象。
    // 为什么能实现：post() 会异步发送请求体，响应回来后通过 QNetworkReply::finished 通知。
    // 什么时候调用：登录请求体准备好后立刻调用。
    // 和谁配合：下面的 finished 槽函数负责读取响应并决定成功或失败。
    QNetworkReply *reply = m_networkManager->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 这是什么：处理临时登录接口返回结果。
        // 为什么能实现：finished 触发时 reply 已经包含网络状态和响应体，可以统一做错误处理和 JSON 解析。
        // 什么时候调用：Qt 事件循环收到 POST /login 完成信号时自动调用。
        // 和谁配合：成功发 loginSucceeded 给 Login，失败发 loginFailed 让 Login 提示用户。
        if (reply->error() != QNetworkReply::NoError) {
            emit loginFailed(reply->errorString());
            reply->deleteLater();
            return;
        }

        const QByteArray data = reply->readAll();
        const QJsonObject obj = QJsonDocument::fromJson(data).object();
        const bool success = obj["success"].toBool(false);
        if (!success) {
            const QString message = obj["message"].toString("账号或密码错误");
            emit loginFailed(message);
            reply->deleteLater();
            return;
        }

        const QString userName = obj["userName"].toString();
        const QString loginAccount = obj["account"].toString();
        if (userName.isEmpty() || loginAccount.isEmpty()) {
            emit loginFailed("登录响应缺少用户信息");
            reply->deleteLater();
            return;
        }

        emit loginSucceeded(userName, loginAccount);
        reply->deleteLater();
    });
}

void ApiClient::uploadVideo(const UploadVideoInfo &info)
{
    // 这是什么：描述一次 POST /videos 上传元数据请求。
    // 为什么能实现：第一版上传只需要 JSON 元数据，QNetworkRequest 设置好 URL 和 Content-Type 后即可发送。
    // 什么时候调用：UploadVideoPage 完成本地表单校验，并准备好当前用户信息后调用。
    // 和谁配合：m_uploadVideoUrl 指向 mock server 或真实后端的视频发布接口。
    QNetworkRequest request(m_uploadVideoUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonArray tagArray;
    for (const QString &tag : info.tags) {
        tagArray.append(tag);
    }

    QJsonObject payload;
    payload["title"] = info.title;
    payload["description"] = info.description;
    payload["category"] = info.category;
    payload["tags"] = tagArray;
    payload["userName"] = info.userName;
    payload["account"] = info.account;
    payload["videoFileName"] = info.videoFileName;
    payload["coverFileName"] = info.coverFileName;
    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    // 这是什么：真正发送上传元数据请求。
    // 为什么能实现：post() 异步发送 JSON，请求结束后通过 finished 信号读取后端发布结果。
    // 什么时候调用：上传请求体构造完成后立刻调用。
    // 和谁配合：下面的 finished 槽函数把响应转成 uploadSucceeded/uploadFailed。
    QNetworkReply *reply = m_networkManager->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 这是什么：处理上传视频元数据接口返回结果。
        // 为什么能实现：finished 触发时 reply 里已有网络状态和响应体，可统一解析 success/message。
        // 什么时候调用：Qt 事件循环收到 POST /videos 完成信号时自动调用。
        // 和谁配合：成功通知上传页收尾，失败通知上传页恢复按钮并显示错误。
        if (reply->error() != QNetworkReply::NoError) {
            emit uploadFailed(reply->errorString());
            reply->deleteLater();
            return;
        }

        const QByteArray data = reply->readAll();
        const QJsonObject obj = QJsonDocument::fromJson(data).object();
        const bool success = obj["success"].toBool(false);
        const QString message = obj["message"].toString(success ? "发布成功" : "发布失败");
        if (!success) {
            emit uploadFailed(message);
            reply->deleteLater();
            return;
        }

        emit uploadSucceeded(message);
        reply->deleteLater();
    });
}
