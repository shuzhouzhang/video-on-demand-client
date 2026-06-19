#include "apiclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

namespace {
VideoInfo videoInfoFromJsonObject(const QJsonObject &obj)
{
    // 这是什么：把单个视频 JSON 对象转换成 VideoInfo。
    // 为什么能实现：详情接口和列表接口使用同一组字段，按字段名读取后即可交给页面使用。
    // 什么时候调用：fetchVideoDetail() 收到 success=true 的 video 对象后调用。
    // 和谁配合：ApiClient 负责解析 JSON，PlayerPage 负责把 VideoInfo 应用到播放页 UI。
    VideoInfo video;
    video.id = obj["id"].toString();
    video.title = obj["title"].toString();
    video.userName = obj["userName"].toString();
    video.date = obj["date"].toString();
    video.duration = obj["duration"].toString();
    video.playCount = obj["playCount"].toString();
    video.likeCount = obj["likeCount"].toString();
    video.category = obj["category"].toString();
    video.description = obj["description"].toString();

    const QJsonArray tagArray = obj["tags"].toArray();
    for (const QJsonValue &tagValue : tagArray) {
        video.tags.append(tagValue.toString());
    }

    return video;
}

UserInfo userInfoFromJsonObject(const QJsonObject &obj)
{
    // 这是什么：把个人资料 JSON 转换成 UserInfo。
    // 为什么能实现：接口字段与 UserInfo 的账号、昵称和简介一一对应。
    // 什么时候调用：个人资料 GET/POST 成功响应需要交给页面时调用。
    // 和谁配合：DataCenter 保存结果，player.cpp 更新“我的”页面。
    UserInfo user;
    user.account = obj["account"].toString();
    user.userName = obj["userName"].toString();
    user.description = obj["description"].toString();
    user.avatarPath = obj["avatarPath"].toString();
    return user;
}
}

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

void ApiClient::requestEmailCode(const QString &email)
{
    // 这是什么：发送邮箱验证码申请请求。
    // 为什么能实现：JSON POST 把邮箱交给后端，后端生成会话 id 和验证码。
    // 什么时候调用：Login 完成邮箱格式校验后调用。
    // 和谁配合：emailCodeSent/emailCodeFailed 把异步结果交回登录窗口。
    QNetworkRequest request(m_emailCodeUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject payload;
    payload["email"] = email.trimmed();
    QNetworkReply *reply = m_networkManager->post(
        request,
        QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit emailCodeFailed(reply->errorString());
            reply->deleteLater();
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QString authcodeId = obj["authcodeId"].toString();
        if (!obj["success"].toBool(false) || authcodeId.isEmpty()) {
            emit emailCodeFailed(obj["message"].toString("验证码发送失败"));
        } else {
            emit emailCodeSent(authcodeId, obj["debugCode"].toString());
        }
        reply->deleteLater();
    });
}

void ApiClient::emailLogin(const QString &email,
                           const QString &authcodeId,
                           const QString &authcode)
{
    // 这是什么：提交邮箱验证码完成登录或首次注册。
    // 为什么能实现：后端用 authcodeId 找到验证码会话，再核对邮箱和六位验证码。
    // 什么时候调用：用户填写邮箱验证码并点击登录时调用。
    // 和谁配合：成功复用 loginSucceeded，失败复用 loginFailed。
    QNetworkRequest request(m_emailLoginUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject payload;
    payload["email"] = email.trimmed();
    payload["authcodeId"] = authcodeId;
    payload["authcode"] = authcode.trimmed();
    QNetworkReply *reply = m_networkManager->post(
        request,
        QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit loginFailed(reply->errorString());
            reply->deleteLater();
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QString userName = obj["userName"].toString();
        const QString account = obj["account"].toString();
        if (!obj["success"].toBool(false) || userName.isEmpty() || account.isEmpty()) {
            emit loginFailed(obj["message"].toString("邮箱验证码登录失败"));
        } else {
            emit loginSucceeded(userName, account);
        }
        reply->deleteLater();
    });
}

void ApiClient::uploadVideo(const UploadVideoInfo &info)
{
    // 这是什么：使用 multipart/form-data 上传视频元数据和真实文件。
    // 为什么能实现：QHttpMultiPart 可在同一个 HTTP 请求中同时承载 JSON、视频二进制和封面二进制。
    // 什么时候调用：上传页完成表单、登录和本地文件校验后调用。
    // 和谁配合：UploadVideoPage 提供路径，mock/后端保存文件并返回创建结果。
    auto *videoFile = new QFile(info.videoFilePath);
    if (!videoFile->open(QIODevice::ReadOnly)) {
        emit uploadFailed("视频文件无法读取");
        delete videoFile;
        return;
    }

    QNetworkRequest request(m_uploadVideoFilesUrl);
    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

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

    QHttpPart metadataPart;
    metadataPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QVariant("form-data; name=\"metadata\""));
    metadataPart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/json"));
    metadataPart.setBody(QJsonDocument(payload).toJson(QJsonDocument::Compact));
    multiPart->append(metadataPart);

    QHttpPart videoPart;
    videoPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                        QVariant(QString("form-data; name=\"videoFile\"; filename=\"%1\"")
                                     .arg(QFileInfo(info.videoFilePath).fileName())));
    videoPart.setBodyDevice(videoFile);
    videoFile->setParent(multiPart);
    multiPart->append(videoPart);

    if (!info.coverFilePath.isEmpty()) {
        auto *coverFile = new QFile(info.coverFilePath);
        if (!coverFile->open(QIODevice::ReadOnly)) {
            emit uploadFailed("封面文件无法读取");
            delete multiPart;
            return;
        }
        QHttpPart coverPart;
        coverPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                            QVariant(QString("form-data; name=\"coverFile\"; filename=\"%1\"")
                                         .arg(QFileInfo(info.coverFilePath).fileName())));
        coverPart.setBodyDevice(coverFile);
        coverFile->setParent(multiPart);
        multiPart->append(coverPart);
    }

    // 这是什么：真正发送包含文件流的 multipart 请求。
    // 为什么能实现：QNetworkAccessManager 会边读取 QFile 边上传，不需要把大视频一次性装进内存。
    // 什么时候调用：metadata、videoFile 和可选 coverFile 都加入 multiPart 后调用。
    // 和谁配合：reply 管理 multiPart 生命周期，finished 回调解析后端结果。
    QNetworkReply *reply = m_networkManager->post(request, multiPart);
    multiPart->setParent(reply);
    connect(reply, &QNetworkReply::uploadProgress, this, [this](qint64 sent, qint64 total) {
        // 这是什么：把 Qt 的上传字节进度转换成页面易用的百分比。
        // 为什么能实现：sent/total 表示当前已发送比例，限制到 0~100 可避免异常值。
        // 什么时候调用：multipart 正在传输时由 QNetworkReply 持续触发。
        // 和谁配合：UploadVideoPage 接收 uploadProgressChanged 更新进度文字。
        if (total > 0) {
            emit uploadProgressChanged(qBound(0, static_cast<int>(sent * 100 / total), 100));
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 这是什么：处理真实文件上传接口返回结果。
        // 为什么能实现：finished 时 multipart 已发送完毕，响应 JSON 会说明文件和元数据是否保存成功。
        // 什么时候调用：Qt 事件循环收到 POST /videos/upload 完成信号时自动调用。
        // 和谁配合：成功发 uploadSucceeded，失败发 uploadFailed；reply 销毁时同时清理文件对象。
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

void ApiClient::fetchPlayUrl(const QString &videoId)
{
    // 这是什么：按 videoId 请求对应视频的播放地址。
    // 为什么能实现：QUrlQuery 把视频标识传给后端，避免所有卡片共用一个无法区分的固定请求。
    // 什么时候调用：PlayerPage 初始化播放器后，需要从接口获取播放地址时调用。
    // 和谁配合：m_playUrlUrl 指向 mock server 或真实后端的播放地址接口。
    const QString trimmedVideoId = videoId.trimmed();
    if (trimmedVideoId.isEmpty()) {
        emit playUrlFailed("视频 id 不能为空");
        return;
    }
    QUrl url = m_playUrlUrl;
    QUrlQuery query;
    query.addQueryItem("videoId", trimmedVideoId);
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 这是什么：真正发起播放地址请求。
    // 为什么能实现：get() 立即返回 QNetworkReply，接口响应稍后通过 finished 信号到达。
    // 什么时候调用：request 准备好后立刻调用。
    // 和谁配合：下面的 finished 槽函数读取 JSON 并发出 playUrlLoaded/playUrlFailed。
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 这是什么：处理播放地址接口返回结果。
        // 为什么能实现：finished 触发时 reply 中已有网络状态和响应体，可以统一解析 success/playUrl。
        // 什么时候调用：Qt 事件循环收到 GET /videos/play-url 完成信号时自动调用。
        // 和谁配合：成功交给 PlayerPage 播放，失败让 PlayerPage 回退本地 test.mp4。
        if (reply->error() != QNetworkReply::NoError) {
            emit playUrlFailed(reply->errorString());
            reply->deleteLater();
            return;
        }

        const QByteArray data = reply->readAll();
        const QJsonObject obj = QJsonDocument::fromJson(data).object();
        const bool success = obj["success"].toBool(false);
        const QString playUrl = obj["playUrl"].toString().trimmed();
        if (!success || playUrl.isEmpty()) {
            const QString message = obj["message"].toString("播放地址为空");
            emit playUrlFailed(message);
            reply->deleteLater();
            return;
        }

        emit playUrlLoaded(playUrl);
        reply->deleteLater();
    });
}

void ApiClient::fetchBarrages(const QString &videoId)
{
    // 这是什么：按 videoId 请求当前视频弹幕。
    // 为什么能实现：后端按视频 id 隔离弹幕集合，不同视频不会互相串数据。
    // 什么时候调用：播放页确定 m_videoId 后调用。
    // 和谁配合：返回的弹幕按秒数整理后交给 PlayerPage/DataCenter。
    const QString trimmedVideoId = videoId.trimmed();
    if (trimmedVideoId.isEmpty()) {
        emit barrageRequestFailed("视频 id 不能为空");
        return;
    }
    QUrl url = m_barragesUrl;
    QUrlQuery query;
    query.addQueryItem("videoId", trimmedVideoId);
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 这是什么：处理弹幕列表接口返回结果。
        // 为什么能实现：finished 时 response body 已完整可读，可以把 JSON 数组整理成按秒分组的哈希表。
        // 什么时候调用：Qt 事件循环收到 GET /videos/barrages 完成信号时自动调用。
        // 和谁配合：PlayerPage 收到 barragesLoaded 后写入 DataCenter。
        if (reply->error() != QNetworkReply::NoError) {
            emit barrageRequestFailed(reply->errorString());
            reply->deleteLater();
            return;
        }

        const QByteArray data = reply->readAll();
        const QJsonObject obj = QJsonDocument::fromJson(data).object();
        if (!obj["success"].toBool(false)) {
            emit barrageRequestFailed(obj["message"].toString("弹幕加载失败"));
            reply->deleteLater();
            return;
        }

        QHash<int, QStringList> barragesBySecond;
        const QJsonArray barrages = obj["barrages"].toArray();
        for (const QJsonValue &value : barrages) {
            const QJsonObject barrage = value.toObject();
            const int seconds = barrage["seconds"].toInt(-1);
            const QString text = barrage["text"].toString().trimmed();
            if (seconds >= 0 && !text.isEmpty()) {
                barragesBySecond[seconds].append(text.left(30));
            }
        }

        emit barragesLoaded(barragesBySecond);
        reply->deleteLater();
    });
}

void ApiClient::sendBarrage(const QString &videoId, int seconds, const QString &text)
{
    // 这是什么：描述一次 POST /videos/barrages 请求。
    // 为什么能实现：发送弹幕只需要 videoId、seconds、text 和用户信息，JSON POST 可写入对应视频集合。
    // 什么时候调用：播放页确认弹幕文本非空且秒数合法后调用。
    // 和谁配合：DataCenter 提供当前用户，mock server 保存弹幕并返回最终文本。
    if (videoId.trimmed().isEmpty()) {
        emit barrageRequestFailed("视频 id 不能为空");
        return;
    }
    QNetworkRequest request(m_barragesUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    const UserInfo currentUser = DataCenter::instance().currentUser();
    QJsonObject payload;
    payload["videoId"] = videoId.trimmed();
    payload["seconds"] = seconds;
    payload["text"] = text;
    payload["userName"] = currentUser.userName;
    payload["account"] = currentUser.account;
    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QNetworkReply *reply = m_networkManager->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 这是什么：处理发送弹幕接口返回结果。
        // 为什么能实现：mock/后端返回 success、seconds、text，成功后页面可以用这些值立即展示弹幕。
        // 什么时候调用：Qt 事件循环收到 POST /videos/barrages 完成信号时自动调用。
        // 和谁配合：PlayerPage 收到 barrageSendSucceeded 后显示弹幕并写入本地缓存。
        if (reply->error() != QNetworkReply::NoError) {
            emit barrageRequestFailed(reply->errorString());
            reply->deleteLater();
            return;
        }

        const QByteArray data = reply->readAll();
        const QJsonObject obj = QJsonDocument::fromJson(data).object();
        if (!obj["success"].toBool(false)) {
            emit barrageRequestFailed(obj["message"].toString("弹幕发送失败"));
            reply->deleteLater();
            return;
        }

        const int seconds = obj["seconds"].toInt(-1);
        const QString text = obj["text"].toString().trimmed();
        if (seconds < 0 || text.isEmpty()) {
            emit barrageRequestFailed("弹幕响应缺少内容");
            reply->deleteLater();
            return;
        }

        emit barrageSendSucceeded(text, seconds);
        reply->deleteLater();
    });
}

void ApiClient::fetchVideoDetail(const QString &videoId)
{
    // 这是什么：准备一次 GET /videos/detail?id=... 请求。
    // 为什么能实现：QUrlQuery 会把 videoId 安全放进 URL query，mock/后端可据此查找对应视频详情。
    // 什么时候调用：播放页打开并拿到首页卡片传来的 videoId 后调用。
    // 和谁配合：m_videoDetailUrl 指向详情接口，下面的 finished 回调负责把响应转成 VideoInfo。
    const QString trimmedVideoId = videoId.trimmed();
    if (trimmedVideoId.isEmpty()) {
        emit videoDetailFailed("视频 id 不能为空");
        return;
    }

    QUrl url = m_videoDetailUrl;
    QUrlQuery query;
    query.addQueryItem("id", trimmedVideoId);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 这是什么：处理视频详情接口返回结果。
        // 为什么能实现：finished 触发时 reply 已包含网络状态和响应体，可以统一判断错误、解析 success/video 字段。
        // 什么时候调用：Qt 事件循环收到 GET /videos/detail 完成信号时自动调用。
        // 和谁配合：成功发 videoDetailLoaded 给 PlayerPage，失败发 videoDetailFailed 让播放页保留兜底信息。
        if (reply->error() != QNetworkReply::NoError) {
            emit videoDetailFailed(reply->errorString());
            reply->deleteLater();
            return;
        }

        const QByteArray data = reply->readAll();
        const QJsonObject obj = QJsonDocument::fromJson(data).object();
        if (!obj["success"].toBool(false)) {
            emit videoDetailFailed(obj["message"].toString("视频详情加载失败"));
            reply->deleteLater();
            return;
        }

        const VideoInfo video = videoInfoFromJsonObject(obj["video"].toObject());
        if (video.title.isEmpty()) {
            emit videoDetailFailed("视频详情响应缺少标题");
            reply->deleteLater();
            return;
        }

        emit videoDetailLoaded(video);
        reply->deleteLater();
    });
}

void ApiClient::likeVideo(const QString &videoId)
{
    // 这是什么：对外提供“点赞当前视频”的接口。
    // 为什么能实现：点赞请求和取消点赞请求共用 JSON 结构，这里只需要选择 /videos/like 地址。
    // 什么时候调用：播放页点赞按钮处于未点赞状态时调用。
    // 和谁配合：sendVideoLikeRequest() 发请求，PlayerPage 接收 videoLikeChanged 后更新 UI。
    sendVideoLikeRequest(m_likeUrl, videoId);
}

void ApiClient::unlikeVideo(const QString &videoId)
{
    // 这是什么：对外提供“取消点赞当前视频”的接口。
    // 为什么能实现：mock/后端用 videoId/account 找到点赞关系并删除，返回最终 liked=false。
    // 什么时候调用：播放页点赞按钮处于已点赞状态时调用。
    // 和谁配合：sendVideoLikeRequest() 发请求，PlayerPage 接收 videoLikeChanged 后更新 UI。
    sendVideoLikeRequest(m_unlikeUrl, videoId);
}

void ApiClient::fetchVideoLikeStatus(const QString &videoId)
{
    // 这是什么：读取当前账号对视频的初始点赞状态和点赞数。
    // 为什么能实现：mock/后端按 account + videoId 查询关系，并从视频数据读取 likeCount。
    // 什么时候调用：播放页初始化时调用一次。
    // 和谁配合：videoLikeStatusLoaded 让 PlayerPage 正确初始化点赞按钮，而非总是假设未点赞。
    const QString trimmedVideoId = videoId.trimmed();
    if (trimmedVideoId.isEmpty()) {
        emit videoLikeFailed("视频 id 不能为空");
        return;
    }
    const UserInfo user = DataCenter::instance().currentUser();
    QUrl url = m_likeStatusUrl;
    QUrlQuery query;
    query.addQueryItem("videoId", trimmedVideoId);
    query.addQueryItem("account", user.account.isEmpty() ? QStringLiteral("guest") : user.account);
    url.setQuery(query);
    QNetworkReply *reply = m_networkManager->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 这是什么：处理点赞状态查询响应。
        // 为什么能实现：响应中的 liked/likeCount 就是页面初始化所需的完整状态。
        // 什么时候调用：GET /videos/like-status 完成后由 Qt 自动调用。
        // 和谁配合：PlayerPage 收到信号后更新 m_isLiked、按钮和数量。
        if (reply->error() != QNetworkReply::NoError) {
            emit videoLikeFailed(reply->errorString());
            reply->deleteLater();
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QString likeCount = obj["likeCount"].toString();
        if (!obj["success"].toBool(false) || likeCount.isEmpty()) {
            emit videoLikeFailed(obj["message"].toString("点赞状态加载失败"));
        } else {
            emit videoLikeStatusLoaded(obj["liked"].toBool(false), likeCount);
        }
        reply->deleteLater();
    });
}

void ApiClient::sendVideoLikeRequest(const QUrl &url, const QString &videoId)
{
    // 这是什么：构造并发送点赞/取消点赞 POST 请求。
    // 为什么能实现：两个接口都只需要 videoId 和 account，QNetworkAccessManager::post() 可以异步发送 JSON。
    // 什么时候调用：likeVideo() 或 unlikeVideo() 选择好接口地址后调用。
    // 和谁配合：DataCenter 提供当前账号，mock/后端返回 liked/likeCount，播放页根据结果更新显示。
    const QString trimmedVideoId = videoId.trimmed();
    if (trimmedVideoId.isEmpty()) {
        emit videoLikeFailed("视频 id 不能为空");
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    const UserInfo currentUser = DataCenter::instance().currentUser();
    QJsonObject payload;
    payload["videoId"] = trimmedVideoId;
    payload["account"] = currentUser.account;
    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QNetworkReply *reply = m_networkManager->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 这是什么：处理点赞/取消点赞接口返回结果。
        // 为什么能实现：finished 触发时 reply 已有完整响应体，可以统一解析 success、liked 和 likeCount。
        // 什么时候调用：Qt 事件循环收到 POST /videos/like 或 /videos/unlike 完成信号时自动调用。
        // 和谁配合：成功发 videoLikeChanged 给 PlayerPage，失败发 videoLikeFailed 保持页面原状态。
        if (reply->error() != QNetworkReply::NoError) {
            emit videoLikeFailed(reply->errorString());
            reply->deleteLater();
            return;
        }

        const QByteArray data = reply->readAll();
        const QJsonObject obj = QJsonDocument::fromJson(data).object();
        if (!obj["success"].toBool(false)) {
            emit videoLikeFailed(obj["message"].toString("点赞请求失败"));
            reply->deleteLater();
            return;
        }

        const bool liked = obj["liked"].toBool(false);
        const QString likeCount = obj["likeCount"].toString();
        if (likeCount.isEmpty()) {
            emit videoLikeFailed("点赞响应缺少点赞数");
            reply->deleteLater();
            return;
        }

        emit videoLikeChanged(liked, likeCount);
        reply->deleteLater();
    });
}

void ApiClient::fetchWatchProgress(const QString &videoId)
{
    // 这是什么：准备一次 GET /videos/watch-progress 请求。
    // 为什么能实现：QUrlQuery 把 videoId/account 放进 URL，mock/后端据此查到当前用户对当前视频的进度。
    // 什么时候调用：播放页打开后，想恢复上次观看位置时调用。
    // 和谁配合：PlayerPage 收到 watchProgressLoaded 后，在 mpv 加载视频后跳转到该秒数。
    const QString trimmedVideoId = videoId.trimmed();
    if (trimmedVideoId.isEmpty()) {
        emit watchProgressFailed("视频 id 不能为空");
        return;
    }

    const UserInfo currentUser = DataCenter::instance().currentUser();
    QUrl url = m_watchProgressUrl;
    QUrlQuery query;
    query.addQueryItem("videoId", trimmedVideoId);
    query.addQueryItem("account", currentUser.account);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 这是什么：处理播放记录读取接口返回结果。
        // 为什么能实现：finished 触发时 reply 已包含完整响应体，可以统一解析 success 和 seconds。
        // 什么时候调用：Qt 事件循环收到 GET /videos/watch-progress 完成信号时自动调用。
        // 和谁配合：成功发 watchProgressLoaded 给 PlayerPage，失败发 watchProgressFailed 但不影响播放。
        if (reply->error() != QNetworkReply::NoError) {
            emit watchProgressFailed(reply->errorString());
            reply->deleteLater();
            return;
        }

        const QByteArray data = reply->readAll();
        const QJsonObject obj = QJsonDocument::fromJson(data).object();
        if (!obj["success"].toBool(false)) {
            emit watchProgressFailed(obj["message"].toString("播放记录加载失败"));
            reply->deleteLater();
            return;
        }

        emit watchProgressLoaded(qMax(0, obj["seconds"].toInt(0)));
        reply->deleteLater();
    });
}

void ApiClient::saveWatchProgress(const QString &videoId, int seconds)
{
    // 这是什么：提交当前视频播放进度。
    // 为什么能实现：播放页已经知道当前播放秒数，POST JSON 即可让 mock/后端按用户和视频保存。
    // 什么时候调用：播放页定时保存，或者窗口隐藏/关闭时保存。
    // 和谁配合：fetchWatchProgress() 下次读取同一份 account + videoId 记录。
    const QString trimmedVideoId = videoId.trimmed();
    if (trimmedVideoId.isEmpty()) {
        emit watchProgressFailed("视频 id 不能为空");
        return;
    }
    if (seconds < 0) {
        emit watchProgressFailed("播放秒数非法");
        return;
    }

    QNetworkRequest request(m_watchProgressUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    const UserInfo currentUser = DataCenter::instance().currentUser();
    QJsonObject payload;
    payload["videoId"] = trimmedVideoId;
    payload["account"] = currentUser.account;
    payload["seconds"] = seconds;
    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QNetworkReply *reply = m_networkManager->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 这是什么：处理播放记录保存接口返回结果。
        // 为什么能实现：mock/后端返回 success/message，成功说明当前秒数已经写入内存记录。
        // 什么时候调用：Qt 事件循环收到 POST /videos/watch-progress 完成信号时自动调用。
        // 和谁配合：PlayerPage 只记录保存结果，播放控制不等待这个接口。
        if (reply->error() != QNetworkReply::NoError) {
            emit watchProgressFailed(reply->errorString());
            reply->deleteLater();
            return;
        }

        const QByteArray data = reply->readAll();
        const QJsonObject obj = QJsonDocument::fromJson(data).object();
        if (!obj["success"].toBool(false)) {
            emit watchProgressFailed(obj["message"].toString("播放记录保存失败"));
            reply->deleteLater();
            return;
        }

        emit watchProgressSaved();
        reply->deleteLater();
    });
}

void ApiClient::fetchComments(const QString &videoId)
{
    // 这是什么：准备一次 GET /videos/comments 请求。
    // 为什么能实现：QUrlQuery 把 videoId 安全编码进 URL，后端据此筛选对应评论。
    // 什么时候调用：评论窗口每次打开并需要刷新列表时调用。
    // 和谁配合：commentsLoaded 把结果交给 CommentDialog，失败则发 commentRequestFailed。
    const QString trimmedVideoId = videoId.trimmed();
    if (trimmedVideoId.isEmpty()) {
        emit commentRequestFailed("视频 id 不能为空");
        return;
    }

    QUrl url = m_commentsUrl;
    QUrlQuery query;
    query.addQueryItem("videoId", trimmedVideoId);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 这是什么：把评论列表响应翻译成 QList<CommentInfo>。
        // 为什么能实现：finished 时响应体已完整，每个 JSON 对象都能按固定字段转成 CommentInfo。
        // 什么时候调用：Qt 事件循环收到 GET /videos/comments 完成信号时自动调用。
        // 和谁配合：成功发 commentsLoaded，CommentDialog 按最新优先顺序渲染。
        if (reply->error() != QNetworkReply::NoError) {
            emit commentRequestFailed(reply->errorString());
            reply->deleteLater();
            return;
        }

        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (!obj["success"].toBool(false)) {
            emit commentRequestFailed(obj["message"].toString("评论加载失败"));
            reply->deleteLater();
            return;
        }

        QList<CommentInfo> comments;
        const QJsonArray items = obj["comments"].toArray();
        comments.reserve(items.size());
        for (const QJsonValue &value : items) {
            const QJsonObject item = value.toObject();
            CommentInfo comment;
            comment.id = item["id"].toString();
            comment.videoId = item["videoId"].toString();
            comment.userName = item["userName"].toString();
            comment.account = item["account"].toString();
            comment.content = item["content"].toString();
            comment.createdAt = item["createdAt"].toString();
            if (!comment.id.isEmpty() && !comment.content.isEmpty()) {
                comments.append(comment);
            }
        }

        emit commentsLoaded(comments);
        reply->deleteLater();
    });
}

void ApiClient::sendComment(const QString &videoId, const QString &content)
{
    // 这是什么：提交一条当前用户的视频评论。
    // 为什么能实现：DataCenter 提供登录身份，QNetworkAccessManager::post() 把身份和正文作为 JSON 发送。
    // 什么时候调用：CommentDialog 校验输入后发出 submitRequested 时调用。
    // 和谁配合：mock/后端生成评论 id 和时间，commentSent 把完整评论交回窗口。
    const QString trimmedVideoId = videoId.trimmed();
    const QString trimmedContent = content.trimmed();
    const UserInfo currentUser = DataCenter::instance().currentUser();
    if (trimmedVideoId.isEmpty()) {
        emit commentRequestFailed("视频 id 不能为空");
        return;
    }
    if (!DataCenter::instance().isLoggedIn()) {
        emit commentRequestFailed("请先登录后再发表评论");
        return;
    }
    if (trimmedContent.isEmpty() || trimmedContent.size() > 200) {
        emit commentRequestFailed("评论内容需为 1 到 200 个字符");
        return;
    }

    QNetworkRequest request(m_commentsUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject payload;
    payload["videoId"] = trimmedVideoId;
    payload["userName"] = currentUser.userName;
    payload["account"] = currentUser.account;
    payload["content"] = trimmedContent;

    QNetworkReply *reply = m_networkManager->post(
        request,
        QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 这是什么：处理发表评论接口的最终结果。
        // 为什么能实现：后端成功响应包含完整 comment 对象，可直接转成页面需要的 CommentInfo。
        // 什么时候调用：Qt 事件循环收到 POST /videos/comments 完成信号时自动调用。
        // 和谁配合：成功发 commentSent，失败发 commentRequestFailed 并让窗口恢复发送按钮。
        if (reply->error() != QNetworkReply::NoError) {
            emit commentRequestFailed(reply->errorString());
            reply->deleteLater();
            return;
        }

        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (!obj["success"].toBool(false)) {
            emit commentRequestFailed(obj["message"].toString("评论发送失败"));
            reply->deleteLater();
            return;
        }

        const QJsonObject item = obj["comment"].toObject();
        CommentInfo comment;
        comment.id = item["id"].toString();
        comment.videoId = item["videoId"].toString();
        comment.userName = item["userName"].toString();
        comment.account = item["account"].toString();
        comment.content = item["content"].toString();
        comment.createdAt = item["createdAt"].toString();
        if (comment.id.isEmpty() || comment.content.isEmpty()) {
            emit commentRequestFailed("评论响应缺少必要字段");
        } else {
            emit commentSent(comment);
        }
        reply->deleteLater();
    });
}

void ApiClient::searchVideos(const QString &keyword)
{
    // 这是什么：准备一次带 keyword 参数的视频搜索 GET 请求。
    // 为什么能实现：QUrlQuery 会安全编码关键词，mock/后端按同名参数执行匹配。
    // 什么时候调用：首页搜索按钮或搜索框回车触发时调用。
    // 和谁配合：searchResultsLoaded 把结果交给 player.cpp，searchFailed 保留当前列表。
    const QString trimmedKeyword = keyword.trimmed();
    if (trimmedKeyword.isEmpty()) {
        emit searchFailed("搜索关键词不能为空");
        return;
    }

    QUrl url = m_searchUrl;
    QUrlQuery query;
    query.addQueryItem("keyword", trimmedKeyword);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 这是什么：处理搜索响应并把视频 JSON 数组转换成 VideoInfo 列表。
        // 为什么能实现：搜索接口沿用 /videos 相同字段，可复用 videoInfoFromJsonObject() 的解析规则。
        // 什么时候调用：Qt 收到 GET /videos/search 完成信号时自动调用。
        // 和谁配合：成功发 searchResultsLoaded，页面用原有 VideoBox 展示；失败发 searchFailed。
        if (reply->error() != QNetworkReply::NoError) {
            emit searchFailed(reply->errorString());
            reply->deleteLater();
            return;
        }

        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (!obj["success"].toBool(false)) {
            emit searchFailed(obj["message"].toString("视频搜索失败"));
            reply->deleteLater();
            return;
        }

        QList<VideoInfo> videos;
        const QJsonArray items = obj["videos"].toArray();
        videos.reserve(items.size());
        for (const QJsonValue &value : items) {
            const VideoInfo video = videoInfoFromJsonObject(value.toObject());
            if (!video.id.isEmpty() && !video.title.isEmpty()) {
                videos.append(video);
            }
        }

        emit searchResultsLoaded(videos);
        reply->deleteLater();
    });
}

void ApiClient::fetchFavoriteStatus(const QString &videoId)
{
    // 这是什么：查询当前账号与视频之间的收藏关系。
    // 为什么能实现：后端用 account + videoId 作为唯一关系键，可返回最终 favorited 状态。
    // 什么时候调用：播放页初始化视频详情和播放记录时一并调用。
    // 和谁配合：DataCenter 提供账号，PlayerPage 接收 videoFavoriteStatusLoaded。
    const QString trimmedVideoId = videoId.trimmed();
    const UserInfo user = DataCenter::instance().currentUser();
    if (trimmedVideoId.isEmpty() || user.account.isEmpty()) {
        emit favoriteRequestFailed("请先登录后查看收藏状态");
        return;
    }

    QUrl url = m_favoriteStatusUrl;
    QUrlQuery query;
    query.addQueryItem("videoId", trimmedVideoId);
    query.addQueryItem("account", user.account);
    url.setQuery(query);
    QNetworkReply *reply = m_networkManager->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit favoriteRequestFailed(reply->errorString());
            reply->deleteLater();
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (!obj["success"].toBool(false)) {
            emit favoriteRequestFailed(obj["message"].toString("收藏状态加载失败"));
        } else {
            emit videoFavoriteStatusLoaded(obj["favorited"].toBool(false));
        }
        reply->deleteLater();
    });
}

void ApiClient::favoriteVideo(const QString &videoId)
{
    // 这是什么：收藏视频的公开入口。
    // 为什么能实现：复用共同 POST 实现，并指定收藏接口地址。
    // 什么时候调用：播放页当前为未收藏状态时调用。
    // 和谁配合：sendVideoFavoriteRequest() 完成请求并发出最终状态。
    sendVideoFavoriteRequest(m_favoriteUrl, videoId);
}

void ApiClient::unfavoriteVideo(const QString &videoId)
{
    // 这是什么：取消收藏视频的公开入口。
    // 为什么能实现：复用共同 POST 实现，并指定取消收藏接口地址。
    // 什么时候调用：播放页当前为已收藏状态时调用。
    // 和谁配合：sendVideoFavoriteRequest() 完成请求并发出最终状态。
    sendVideoFavoriteRequest(m_unfavoriteUrl, videoId);
}

void ApiClient::sendVideoFavoriteRequest(const QUrl &url, const QString &videoId)
{
    // 这是什么：发送收藏或取消收藏请求的共同实现。
    // 为什么能实现：两个接口都接收 videoId/account，并返回最终 favorited 状态。
    // 什么时候调用：用户点击播放页收藏按钮后，由 favoriteVideo()/unfavoriteVideo() 调用。
    // 和谁配合：DataCenter 提供登录账号，PlayerPage 接收 videoFavoriteChanged。
    const QString trimmedVideoId = videoId.trimmed();
    const UserInfo user = DataCenter::instance().currentUser();
    if (trimmedVideoId.isEmpty()) {
        emit favoriteRequestFailed("视频 id 不能为空");
        return;
    }
    if (user.account.isEmpty()) {
        emit favoriteRequestFailed("请先登录后再收藏视频");
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject payload;
    payload["videoId"] = trimmedVideoId;
    payload["account"] = user.account;
    QNetworkReply *reply = m_networkManager->post(
        request,
        QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit favoriteRequestFailed(reply->errorString());
            reply->deleteLater();
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (!obj["success"].toBool(false)) {
            emit favoriteRequestFailed(obj["message"].toString("收藏操作失败"));
        } else {
            emit videoFavoriteChanged(obj["favorited"].toBool(false));
        }
        reply->deleteLater();
    });
}

void ApiClient::fetchFavoriteVideos()
{
    // 这是什么：读取当前用户全部收藏视频。
    // 为什么能实现：后端根据 account 找到收藏 videoId，再映射回完整 VIDEOS 数据。
    // 什么时候调用：用户进入“我的”页面并点击“我的收藏”时调用。
    // 和谁配合：favoriteVideosLoaded 把 VideoInfo 列表交给 player.cpp 渲染。
    const UserInfo user = DataCenter::instance().currentUser();
    if (user.account.isEmpty()) {
        emit favoriteRequestFailed("请先登录后查看收藏");
        return;
    }

    QUrl url = m_favoriteVideosUrl;
    QUrlQuery query;
    query.addQueryItem("account", user.account);
    url.setQuery(query);
    QNetworkReply *reply = m_networkManager->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit favoriteRequestFailed(reply->errorString());
            reply->deleteLater();
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (!obj["success"].toBool(false)) {
            emit favoriteRequestFailed(obj["message"].toString("收藏列表加载失败"));
            reply->deleteLater();
            return;
        }

        QList<VideoInfo> videos;
        const QJsonArray items = obj["videos"].toArray();
        for (const QJsonValue &value : items) {
            const VideoInfo video = videoInfoFromJsonObject(value.toObject());
            if (!video.id.isEmpty()) {
                videos.append(video);
            }
        }
        emit favoriteVideosLoaded(videos);
        reply->deleteLater();
    });
}

void ApiClient::fetchUserProfile()
{
    // 这是什么：读取当前账号的个人资料。
    // 为什么能实现：账号作为 query 参数发送，后端可在 USERS 中找到对应资料。
    // 什么时候调用：登录成功后需要用后端最新资料刷新“我的”页面时调用。
    // 和谁配合：userProfileLoaded 返回 UserInfo，DataCenter 保存共享状态。
    const UserInfo currentUser = DataCenter::instance().currentUser();
    if (currentUser.account.isEmpty()) {
        emit userProfileFailed("请先登录后查看资料");
        return;
    }

    QUrl url = m_userProfileUrl;
    QUrlQuery query;
    query.addQueryItem("account", currentUser.account);
    url.setQuery(query);
    QNetworkReply *reply = m_networkManager->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit userProfileFailed(reply->errorString());
            reply->deleteLater();
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (!obj["success"].toBool(false)) {
            emit userProfileFailed(obj["message"].toString("个人资料加载失败"));
        } else {
            const UserInfo user = userInfoFromJsonObject(obj["user"].toObject());
            if (user.account.isEmpty() || user.userName.isEmpty()) {
                emit userProfileFailed("个人资料响应缺少必要字段");
            } else {
                emit userProfileLoaded(user);
            }
        }
        reply->deleteLater();
    });
}

void ApiClient::updateUserProfile(const QString &userName, const QString &description)
{
    // 这是什么：提交当前用户修改后的昵称和简介。
    // 为什么能实现：POST JSON 携带账号定位用户，后端校验后覆盖对应资料字段。
    // 什么时候调用：ProfileDialog 发出 saveRequested 时调用。
    // 和谁配合：userProfileUpdated 返回最终资料，页面和 DataCenter 同步更新。
    const UserInfo currentUser = DataCenter::instance().currentUser();
    const QString trimmedName = userName.trimmed();
    const QString trimmedDescription = description.trimmed();
    if (currentUser.account.isEmpty()) {
        emit userProfileFailed("请先登录后修改资料");
        return;
    }
    if (trimmedName.isEmpty() || trimmedName.size() > 20) {
        emit userProfileFailed("昵称需为 1 到 20 个字符");
        return;
    }
    if (trimmedDescription.size() > 100) {
        emit userProfileFailed("个人简介不能超过 100 个字符");
        return;
    }

    QNetworkRequest request(m_userProfileUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject payload;
    payload["account"] = currentUser.account;
    payload["userName"] = trimmedName;
    payload["description"] = trimmedDescription;
    QNetworkReply *reply = m_networkManager->post(
        request,
        QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit userProfileFailed(reply->errorString());
            reply->deleteLater();
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (!obj["success"].toBool(false)) {
            emit userProfileFailed(obj["message"].toString("个人资料保存失败"));
        } else {
            const UserInfo user = userInfoFromJsonObject(obj["user"].toObject());
            if (user.account.isEmpty() || user.userName.isEmpty()) {
                emit userProfileFailed("个人资料响应缺少必要字段");
            } else {
                emit userProfileUpdated(user);
            }
        }
        reply->deleteLater();
    });
}

void ApiClient::fetchMyVideos()
{
    // 这是什么：读取当前账号发布的全部视频。
    // 为什么能实现：account 放入 GET 参数，后端根据 ownerAccount 筛选并返回标准视频数组。
    // 什么时候调用：用户点击“我的视频”或刚上传成功回到个人页时调用。
    // 和谁配合：myVideosLoaded 把结果交给 player.cpp 的通用视频列表渲染函数。
    const UserInfo user = DataCenter::instance().currentUser();
    if (user.account.isEmpty()) {
        emit myVideosFailed("请先登录后查看作品");
        return;
    }

    QUrl url = m_myVideosUrl;
    QUrlQuery query;
    query.addQueryItem("account", user.account);
    url.setQuery(query);
    QNetworkReply *reply = m_networkManager->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 这是什么：处理我的视频接口响应。
        // 为什么能实现：每一项字段与 VideoInfo 一致，可复用统一 JSON 转换函数。
        // 什么时候调用：GET /users/videos 完成后由 Qt 自动调用。
        // 和谁配合：成功发 myVideosLoaded，失败发 myVideosFailed。
        if (reply->error() != QNetworkReply::NoError) {
            emit myVideosFailed(reply->errorString());
            reply->deleteLater();
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (!obj["success"].toBool(false)) {
            emit myVideosFailed(obj["message"].toString("我的视频加载失败"));
            reply->deleteLater();
            return;
        }
        QList<VideoInfo> videos;
        for (const QJsonValue &value : obj["videos"].toArray()) {
            const VideoInfo video = videoInfoFromJsonObject(value.toObject());
            if (!video.id.isEmpty()) {
                videos.append(video);
            }
        }
        emit myVideosLoaded(videos);
        reply->deleteLater();
    });
}

void ApiClient::uploadAvatar(const QString &filePath)
{
    // 这是什么：使用 multipart 上传当前用户头像。
    // 为什么能实现：账号表单字段定位用户，avatarFile 部分承载真实图片字节。
    // 什么时候调用：用户选择不超过 5MB 的有效图片后调用。
    // 和谁配合：mock 保存文件并更新 USERS，avatarUploaded 把路径交回 player.cpp。
    const UserInfo user = DataCenter::instance().currentUser();
    if (user.account.isEmpty()) {
        emit avatarUploadFailed("请先登录后修改头像");
        return;
    }
    auto *avatarFile = new QFile(filePath);
    if (!avatarFile->open(QIODevice::ReadOnly)) {
        emit avatarUploadFailed("头像文件无法读取");
        delete avatarFile;
        return;
    }

    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart accountPart;
    accountPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                          QVariant("form-data; name=\"account\""));
    accountPart.setBody(user.account.toUtf8());
    multiPart->append(accountPart);

    QHttpPart avatarPart;
    avatarPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                         QVariant(QString("form-data; name=\"avatarFile\"; filename=\"%1\"")
                                      .arg(QFileInfo(filePath).fileName())));
    avatarPart.setBodyDevice(avatarFile);
    avatarFile->setParent(multiPart);
    multiPart->append(avatarPart);

    QNetworkReply *reply = m_networkManager->post(QNetworkRequest(m_avatarUploadUrl), multiPart);
    multiPart->setParent(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 这是什么：处理头像上传响应。
        // 为什么能实现：后端 success=true 时同时返回可供客户端读取的 avatarPath。
        // 什么时候调用：POST /users/avatar 完成后由 Qt 自动调用。
        // 和谁配合：player.cpp 使用 avatarUploaded 更新共享状态和 UI。
        if (reply->error() != QNetworkReply::NoError) {
            emit avatarUploadFailed(reply->errorString());
            reply->deleteLater();
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QString avatarPath = obj["avatarPath"].toString();
        if (!obj["success"].toBool(false) || avatarPath.isEmpty()) {
            emit avatarUploadFailed(obj["message"].toString("头像上传失败"));
        } else {
            emit avatarUploaded(avatarPath);
        }
        reply->deleteLater();
    });
}
