#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <QList>
#include <QString>
#include <QUrl>

#include "datacenter.h"

class QNetworkAccessManager;

// ApiClient 是客户端和后端接口之间的入口；它把网络细节从页面代码里拿出来。
// 这样页面只关心“视频列表加载好了没有”，不直接处理 HTTP、JSON 和错误。
class ApiClient : public QObject
{
    Q_OBJECT

public:
    explicit ApiClient(QObject *parent = nullptr);

    // fetchVideos 会异步请求首页视频列表；调用后不会马上返回数据。
    // 请求完成时，它会和 parseVideosFromJson 配合，把 JSON 转成 VideoInfo 后发出 videosLoaded。
    void fetchVideos();

signals:
    // videosLoaded 和首页配合：ApiClient 拿到视频后通知 player.cpp 刷新 VideoBox。
    void videosLoaded(const QList<VideoInfo> &videos);

    // requestFailed 和首页兜底逻辑配合：请求失败时页面保留本地假数据继续展示。
    void requestFailed(const QString &message);

private:
    QUrl m_videosUrl = QUrl("http://127.0.0.1:8080/videos");
    QNetworkAccessManager *m_networkManager = nullptr;
};

#endif // APICLIENT_H
