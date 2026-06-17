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

    // 这是什么：请求当前视频的弹幕列表。
    // 为什么能实现：mock/后端提供 GET /videos/barrages，返回每条弹幕的秒数和文本。
    // 什么时候调用：播放页拿到 m_videoKey 并准备播放后调用。
    // 和谁配合：PlayerPage 收到 barragesLoaded 后写入 DataCenter，并按播放时间显示。
    void fetchBarrages(const QString &videoKey);

    // 这是什么：发送当前视频的一条弹幕。
    // 为什么能实现：把 videoKey、seconds、text 和当前用户信息组装成 JSON，POST 到 /videos/barrages。
    // 什么时候调用：用户在播放页输入弹幕并点击发送或按回车时调用。
    // 和谁配合：PlayerPage 负责输入校验和成功后的立即显示。
    void sendBarrage(const QString &videoKey, int seconds, const QString &text);

    // 这是什么：请求某个视频的详情信息。
    // 为什么能实现：播放页已经从首页卡片拿到 videoId，ApiClient 可以把它作为 query 参数请求 /videos/detail。
    // 什么时候调用：PlayerPage 打开后，需要用后端/mock 的最新详情刷新标题、作者、播放量和简介时调用。
    // 和谁配合：PlayerPage 收到 videoDetailLoaded 后更新播放页文字，失败时继续使用卡片传入的兜底信息。
    void fetchVideoDetail(const QString &videoId);

    // 这是什么：请求点赞当前视频。
    // 为什么能实现：播放页已经持有 videoId，ApiClient 从 DataCenter 读取当前账号后 POST 到 /videos/like。
    // 什么时候调用：用户在播放页点击未点赞状态的点赞按钮时调用。
    // 和谁配合：PlayerPage 收到 videoLikeChanged 后更新按钮状态和点赞数。
    void likeVideo(const QString &videoId);

    // 这是什么：请求取消点赞当前视频。
    // 为什么能实现：取消点赞和点赞使用同一套 videoId/account 请求体，只是 POST 到 /videos/unlike。
    // 什么时候调用：用户在播放页点击已点赞状态的点赞按钮时调用。
    // 和谁配合：PlayerPage 收到 videoLikeChanged 后恢复按钮状态并更新点赞数。
    void unlikeVideo(const QString &videoId);

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

    // 这是什么：弹幕列表接口成功后的通知信号。
    // 为什么能实现：ApiClient 把返回数组整理成 QHash<int, QStringList>，页面可直接按秒数缓存。
    // 什么时候触发：fetchBarrages() 成功解析出弹幕列表后触发。
    // 和谁配合：PlayerPage 写入 DataCenter，并由 showBarragesAt() 按播放进度显示。
    void barragesLoaded(const QHash<int, QStringList> &barragesBySecond);

    // 这是什么：发送弹幕成功后的通知信号。
    // 为什么能实现：POST /videos/barrages 返回 success=true 后，ApiClient 把最终文本和秒数交回页面。
    // 什么时候触发：sendBarrage() 收到成功响应后触发。
    // 和谁配合：PlayerPage 立即显示这条弹幕，并写入 DataCenter 本地缓存。
    void barrageSendSucceeded(const QString &text, int seconds);

    // 这是什么：弹幕接口失败后的统一通知信号。
    // 为什么能实现：拉取失败、发送失败、网络错误或响应异常都能转成 message。
    // 什么时候触发：fetchBarrages() 或 sendBarrage() 无法完成时触发。
    // 和谁配合：PlayerPage 记录日志或提示，但不影响播放控制。
    void barrageRequestFailed(const QString &message);

    // 这是什么：视频详情接口成功后的通知信号。
    // 为什么能实现：ApiClient 把 /videos/detail 返回的 JSON 翻译成 VideoInfo，页面可直接按字段更新 UI。
    // 什么时候触发：fetchVideoDetail() 成功拿到 success=true 且视频标题非空的响应后触发。
    // 和谁配合：PlayerPage 连接这个信号，刷新标题、作者、日期、播放量、点赞数和简介。
    void videoDetailLoaded(const VideoInfo &video);

    // 这是什么：视频详情接口失败后的通知信号。
    // 为什么能实现：网络错误、缺少 videoId、找不到视频或响应格式异常都会统一转成 message。
    // 什么时候触发：fetchVideoDetail() 无法拿到有效详情时触发。
    // 和谁配合：PlayerPage 记录日志，并继续显示首页卡片传入的兜底信息。
    void videoDetailFailed(const QString &message);

    // 这是什么：点赞状态变化成功后的通知信号。
    // 为什么能实现：mock/后端返回 liked 和 likeCount，ApiClient 可以把最终状态交回播放页。
    // 什么时候触发：likeVideo() 或 unlikeVideo() 收到成功响应后触发。
    // 和谁配合：PlayerPage 用 liked 更新按钮样式，用 likeCount 更新点赞数文本。
    void videoLikeChanged(bool liked, const QString &likeCount);

    // 这是什么：点赞接口失败后的通知信号。
    // 为什么能实现：网络错误、缺少 videoId 或后端返回失败都会统一转成 message。
    // 什么时候触发：点赞或取消点赞请求无法完成时触发。
    // 和谁配合：PlayerPage 记录日志，并保留当前点赞状态不变。
    void videoLikeFailed(const QString &message);

private:
    // 这是什么：点赞和取消点赞共用的 POST 请求实现。
    // 为什么这样做：两个接口请求体和响应解析几乎一样，集中到一个函数可以减少重复和不一致。
    // 什么时候调用：likeVideo() 和 unlikeVideo() 分别传入不同 URL 后调用。
    // 和谁配合：m_likeUrl/m_unlikeUrl 提供接口地址，videoLikeChanged/videoLikeFailed 通知播放页。
    void sendVideoLikeRequest(const QUrl &url, const QString &videoId);

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

    // 这是什么：当前弹幕接口地址。
    // 为什么这样做：第一版沿用固定路径，GET 拉取弹幕，POST 发送弹幕。
    // 什么时候使用：fetchBarrages() 和 sendBarrage() 创建请求时使用。
    // 和谁配合：tools/mock_videos_server.py 的 /videos/barrages 内存接口。
    QUrl m_barragesUrl = QUrl("http://127.0.0.1:8080/videos/barrages");

    // 这是什么：当前视频详情接口地址。
    // 为什么这样做：第一版用固定 /videos/detail 路径配合 id query，后续接真实后端时容易升级为按视频 id 查详情。
    // 什么时候使用：fetchVideoDetail() 创建 GET /videos/detail?id=... 请求时使用。
    // 和谁配合：tools/mock_videos_server.py 根据 id 返回 mock 视频详情。
    QUrl m_videoDetailUrl = QUrl("http://127.0.0.1:8080/videos/detail");

    // 这是什么：当前点赞接口地址。
    // 为什么这样做：第一版用 POST /videos/like 表达“当前用户点赞当前视频”的动作。
    // 什么时候使用：likeVideo() 创建请求时使用。
    // 和谁配合：tools/mock_videos_server.py 在内存中记录用户点赞状态。
    QUrl m_likeUrl = QUrl("http://127.0.0.1:8080/videos/like");

    // 这是什么：当前取消点赞接口地址。
    // 为什么这样做：第一版用 POST /videos/unlike 表达“当前用户取消点赞当前视频”的动作。
    // 什么时候使用：unlikeVideo() 创建请求时使用。
    // 和谁配合：tools/mock_videos_server.py 在内存中移除用户点赞状态。
    QUrl m_unlikeUrl = QUrl("http://127.0.0.1:8080/videos/unlike");

    // 这是什么：Qt 网络请求管理器。
    // 为什么能实现：它负责创建并发送 GET/POST 等请求，返回 QNetworkReply 表示异步响应。
    // 什么时候使用：每次 fetchVideos() 发请求时使用。
    // 和谁配合：QNetworkRequest 描述接口地址，QNetworkReply 承载后端响应。
    QNetworkAccessManager *m_networkManager = nullptr;
};

#endif // APICLIENT_H
