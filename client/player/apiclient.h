#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "datacenter.h"

class QNetworkAccessManager;

// 这是什么：上传视频接口第一版需要发送的元数据。
// 为什么这样做：第一阶段只联调表单字段，不传真实文件二进制，用结构体把页面字段整理后交给 ApiClient。
// 什么时候使用：上传页表单校验通过后创建它，并调用 ApiClient::uploadVideo()。
// 和谁配合：UploadVideoPage 负责填充字段，mock/后端 POST /videos 负责接收 JSON。
struct UploadVideoInfo {
    QString title;
    QString description;
    QString category;
    QStringList tags;
    QString userName;
    QString account;
    QString videoFileName;
    QString coverFileName;
};

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

    // 这是什么：请求临时用户登录接口。
    // 为什么能实现：把账号密码组装成 JSON，用 QNetworkAccessManager::post() 发送到 mock/后端的 /login。
    // 什么时候调用：登录窗口密码登录校验通过后调用。
    // 和谁配合：Login 负责收集输入和展示结果，ApiClient 负责发送请求并发出登录成功/失败信号。
    void login(const QString &account, const QString &password);

    // 这是什么：请求上传视频元数据接口。
    // 为什么能实现：把上传页整理好的 UploadVideoInfo 转成 JSON，用 POST /videos 发给 mock/后端。
    // 什么时候调用：上传页表单校验通过，并确认当前用户已登录后调用。
    // 和谁配合：UploadVideoPage 收集表单并响应 uploadSucceeded/uploadFailed 信号。
    void uploadVideo(const UploadVideoInfo &info);

    // 这是什么：请求最小版视频播放地址接口。
    // 为什么能实现：mock/后端提供 GET /videos/play-url，返回 JSON 里的 playUrl 字段给播放器使用。
    // 什么时候调用：播放页初始化 mpv 后，需要拿到真实播放地址时调用。
    // 和谁配合：PlayerPage 收到 playUrlLoaded 后调用 MpvPlayer::startPlay()。
    void fetchPlayUrl();

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

    // 这是什么：临时登录接口成功后的通知信号。
    // 为什么能实现：/login 响应 JSON 中包含 success、userName、account，解析成功后可以把用户信息交回界面。
    // 什么时候触发：login() 收到 success=true 且 userName/account 非空的响应后触发。
    // 和谁配合：Login 收到后继续发已有 loginSuccess 信号，让 player.cpp 更新“我的”页面。
    void loginSucceeded(const QString &userName, const QString &account);

    // 这是什么：临时登录接口失败后的通知信号。
    // 为什么能实现：网络错误、JSON 格式错误或 success=false 都会产生可展示的错误信息。
    // 什么时候触发：login() 请求失败、响应异常或账号密码不匹配时触发。
    // 和谁配合：Login 收到后恢复登录按钮并弹出提示。
    void loginFailed(const QString &message);

    // 这是什么：上传视频元数据成功后的通知信号。
    // 为什么能实现：POST /videos 返回 success=true 时，ApiClient 可以把 message 交回上传页。
    // 什么时候触发：uploadVideo() 收到成功响应并解析出 message 后触发。
    // 和谁配合：UploadVideoPage 收到后提示成功、重置页面并返回“我的”页。
    void uploadSucceeded(const QString &message);

    // 这是什么：上传视频元数据失败后的通知信号。
    // 为什么能实现：网络错误、JSON 异常或 success=false 都会被统一转成可展示的 message。
    // 什么时候触发：uploadVideo() 请求失败或后端拒绝本次发布时触发。
    // 和谁配合：UploadVideoPage 收到后恢复发布按钮并提示错误原因。
    void uploadFailed(const QString &message);

    // 这是什么：播放地址接口成功后的通知信号。
    // 为什么能实现：GET /videos/play-url 返回 success=true 和 playUrl 后，ApiClient 可以把地址交给播放页。
    // 什么时候触发：fetchPlayUrl() 收到非空 playUrl 后触发。
    // 和谁配合：PlayerPage 用 playUrl 启动 MpvPlayer 播放。
    void playUrlLoaded(const QString &playUrl);

    // 这是什么：播放地址接口失败后的通知信号。
    // 为什么能实现：网络错误、JSON 异常、success=false 或 playUrl 为空都会转成失败消息。
    // 什么时候触发：fetchPlayUrl() 无法拿到可播放地址时触发。
    // 和谁配合：PlayerPage 收到后回退到本地 test.mp4。
    void playUrlFailed(const QString &message);

private:
    // 这是什么：当前首页视频列表接口地址。
    // 为什么这样做：先固定到 mock server，后续接真实后端时只需要替换 baseUrl 或配置来源。
    // 什么时候使用：fetchVideos() 创建 QNetworkRequest 时使用。
    // 和谁配合：tools/mock_videos_server.py 当前提供同路径的 /videos 响应。
    QUrl m_videosUrl = QUrl("http://127.0.0.1:8080/videos");

    // 这是什么：当前临时登录接口地址。
    // 为什么这样做：先固定到 mock server，后续接真实后端时只需要替换这里或抽出 baseUrl。
    // 什么时候使用：login() 创建 POST /login 请求时使用。
    // 和谁配合：tools/mock_videos_server.py 提供同路径的临时登录响应。
    QUrl m_loginUrl = QUrl("http://127.0.0.1:8080/login");

    // 这是什么：当前上传视频元数据接口地址。
    // 为什么这样做：第一版沿用 REST 风格，GET /videos 获取列表，POST /videos 发布新视频元数据。
    // 什么时候使用：uploadVideo() 创建 POST /videos 请求时使用。
    // 和谁配合：tools/mock_videos_server.py 处理同路径的上传请求。
    QUrl m_uploadVideoUrl = QUrl("http://127.0.0.1:8080/videos");

    // 这是什么：当前最小版播放地址接口地址。
    // 为什么这样做：先不引入 videoId，固定接口能最快验证播放页从网络拿播放地址。
    // 什么时候使用：fetchPlayUrl() 创建 GET /videos/play-url 请求时使用。
    // 和谁配合：tools/mock_videos_server.py 返回本地 test.mp4 路径。
    QUrl m_playUrlUrl = QUrl("http://127.0.0.1:8080/videos/play-url");

    // 这是什么：Qt 网络请求管理器。
    // 为什么能实现：它负责创建并发送 GET/POST 等请求，返回 QNetworkReply 表示异步响应。
    // 什么时候使用：每次 fetchVideos() 发请求时使用。
    // 和谁配合：QNetworkRequest 描述接口地址，QNetworkReply 承载后端响应。
    QNetworkAccessManager *m_networkManager = nullptr;
};

#endif // APICLIENT_H
