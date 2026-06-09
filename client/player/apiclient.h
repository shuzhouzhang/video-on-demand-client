#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <QList>
#include <QString>
#include <QUrl>

#include "datacenter.h"

class QNetworkAccessManager;

// 这是什么：ApiClient 是客户端访问后端接口的统一入口。
// 为什么这样做：把 HTTP 请求、响应读取、JSON 解析从页面代码里拆出来，避免 player.cpp 既管 UI 又管网络。
// 什么时候调用：页面需要后端数据时调用它的接口函数，例如首页初始化时调用 fetchVideos()。
// 和谁配合：它和 QNetworkAccessManager、parseVideosFromJson、player.cpp 的信号槽一起完成“接口数据 -> 页面刷新”。
class ApiClient : public QObject
{
    Q_OBJECT

public:
    // 这是什么：构造 ApiClient，并创建 Qt 的网络请求管理器。
    // 为什么能实现：QNetworkAccessManager 是 Qt 发 HTTP 请求的核心对象，作为子对象挂在 this 下可自动释放。
    // 什么时候调用：player.cpp 创建 m_apiClient 时调用一次。
    // 和谁配合：后续 fetchVideos() 会复用 m_networkManager 发 GET 请求。
    explicit ApiClient(QObject *parent = nullptr);

    // 这是什么：请求首页视频列表接口。
    // 为什么能实现：用 QNetworkAccessManager 发 GET /videos，等 QNetworkReply 完成后读取 JSON 并转成 VideoInfo。
    // 什么时候调用：首页初始化完成并先渲染本地兜底数据后调用；后续也可用于刷新首页列表。
    // 和谁配合：请求成功发 videosLoaded 给 player.cpp，请求失败发 requestFailed 让页面继续使用本地兜底数据。
    void fetchVideos();

signals:
    // 这是什么：视频接口请求成功后的通知信号。
    // 为什么能实现：Qt 信号槽允许网络回调完成后把 QList<VideoInfo> 异步交给页面。
    // 什么时候触发：fetchVideos() 读取并解析出非空视频列表后触发。
    // 和谁配合：player.cpp 连接到 setHomeVideos()，收到后刷新 VideoBox。
    void videosLoaded(const QList<VideoInfo> &videos);

    // 这是什么：视频接口请求失败后的通知信号。
    // 为什么能实现：QNetworkReply 能告诉我们网络错误，解析结果为空时也可以主动视为失败。
    // 什么时候触发：后端没启动、接口地址错误、网络异常或 JSON 无法转成有效视频列表时触发。
    // 和谁配合：player.cpp 记录日志并保留 DataCenter::homeVideos() 作为兜底展示。
    void requestFailed(const QString &message);

private:
    // 这是什么：当前首页视频列表接口地址。
    // 为什么这样做：先固定到 mock server，后续接真实后端时只需要替换 baseUrl 或配置来源。
    // 什么时候使用：fetchVideos() 创建 QNetworkRequest 时使用。
    // 和谁配合：tools/mock_videos_server.py 当前提供同路径的 /videos 响应。
    QUrl m_videosUrl = QUrl("http://127.0.0.1:8080/videos");

    // 这是什么：Qt 网络请求管理器。
    // 为什么能实现：它负责创建并发送 GET/POST 等请求，返回 QNetworkReply 表示异步响应。
    // 什么时候使用：每次 fetchVideos() 发请求时使用。
    // 和谁配合：QNetworkRequest 描述接口地址，QNetworkReply 承载后端响应。
    QNetworkAccessManager *m_networkManager = nullptr;
};

#endif // APICLIENT_H
