#include "apiclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

void ApiClient::fetchPlayUrl()
{
    // 这是什么：描述一次 GET /videos/play-url 请求。
    // 为什么能实现：QNetworkRequest 保存播放地址接口 URL，QNetworkAccessManager 负责异步发送 GET。
    // 什么时候调用：PlayerPage 初始化播放器后，需要从接口获取播放地址时调用。
    // 和谁配合：m_playUrlUrl 指向 mock server 或真实后端的播放地址接口。
    QNetworkRequest request(m_playUrlUrl);
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

void ApiClient::fetchBarrages(const QString &videoKey)
{
    // 这是什么：描述一次 GET /videos/barrages 请求。
    // 为什么能实现：第一版 mock 弹幕接口暂不依赖 query 参数，播放页只需请求固定地址即可拿到测试弹幕。
    // 什么时候调用：播放页确定 m_videoKey 后调用。
    // 和谁配合：返回的弹幕按秒数整理后交给 PlayerPage/DataCenter。
    Q_UNUSED(videoKey);
    QNetworkRequest request(m_barragesUrl);
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

void ApiClient::sendBarrage(const QString &videoKey, int seconds, const QString &text)
{
    // 这是什么：描述一次 POST /videos/barrages 请求。
    // 为什么能实现：发送弹幕只需要 videoKey、seconds、text 和用户信息，JSON POST 足够完成第一版联调。
    // 什么时候调用：播放页确认弹幕文本非空且秒数合法后调用。
    // 和谁配合：DataCenter 提供当前用户，mock server 保存弹幕并返回最终文本。
    QNetworkRequest request(m_barragesUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    const UserInfo currentUser = DataCenter::instance().currentUser();
    QJsonObject payload;
    payload["videoKey"] = videoKey;
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
