#include "apiclient.h"

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
