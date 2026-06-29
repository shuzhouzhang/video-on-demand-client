#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "datacenter.h"

class QNetworkAccessManager;

// 这是什么：真实视频上传需要的表单元数据和本地文件路径。
// 为什么这样做：结构体把页面字段、视频路径和封面路径统一交给 ApiClient 组装 multipart 请求。
// 什么时候使用：上传页表单校验通过后创建它，并调用 ApiClient::uploadVideo()。
// 和谁配合：UploadVideoPage 负责填充，mock/后端 POST /videos/upload 接收 JSON 和二进制文件。
struct UploadVideoInfo {
    QString title;
    QString description;
    QString category;
    QStringList tags;
    QString userName;
    QString account;
    QString videoFileName;
    QString coverFileName;
    QString videoFilePath;
    QString coverFilePath;
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

    // 这是什么：向后端申请邮箱登录验证码。
    // 为什么能实现：POST 邮箱后，后端创建验证码会话并返回 authcodeId；mock 额外返回测试验证码。
    // 什么时候调用：邮箱格式校验通过并点击“获取验证码”时调用。
    // 和谁配合：Login 保存 authcodeId，后续 emailLogin() 提交验证码。
    void requestEmailCode(const QString &email);

    // 这是什么：使用邮箱、验证码和验证码会话完成登录或首次注册。
    // 为什么能实现：后端校验三者匹配后返回统一 userName/account 登录结果。
    // 什么时候调用：邮箱登录表单校验通过并点击登录时调用。
    // 和谁配合：复用 loginSucceeded/loginFailed，让 Login 后续流程保持一致。
    void emailLogin(const QString &email, const QString &authcodeId, const QString &authcode);

    // 这是什么：通知后端当前账号退出登录。
    // 为什么能实现：POST account 后，后端可清理真实系统中的会话；mock 返回确认结果。
    // 什么时候调用：已登录用户在设置入口确认退出时调用。
    // 和谁配合：logoutSucceeded 后 player.cpp 清空 DataCenter 和页面状态。
    void logout();

    // 这是什么：请求真实视频文件上传接口。
    // 为什么能实现：把 UploadVideoInfo 拆成 JSON metadata、videoFile 和可选 coverFile multipart 部分。
    // 什么时候调用：上传页表单校验通过，并确认当前用户已登录后调用。
    // 和谁配合：UploadVideoPage 收集表单并响应 uploadSucceeded/uploadFailed 信号。
    void uploadVideo(const UploadVideoInfo &info);

    // 这是什么：请求最小版视频播放地址接口。
    // 为什么能实现：mock/后端提供 GET /videos/play-url，返回 JSON 里的 playUrl 字段给播放器使用。
    // 什么时候调用：播放页初始化 mpv 后，需要拿到真实播放地址时调用。
    // 和谁配合：PlayerPage 收到 playUrlLoaded 后调用 MpvPlayer::startPlay()。
    void fetchPlayUrl(const QString &videoId);

    // 这是什么：请求当前视频的弹幕列表。
    // 为什么能实现：mock/后端提供 GET /videos/barrages，返回每条弹幕的秒数和文本。
    // 什么时候调用：播放页拿到 m_videoId 并准备播放后调用。
    // 和谁配合：PlayerPage 收到 barragesLoaded 后写入 DataCenter，并按播放时间显示。
    void fetchBarrages(const QString &videoId);

    // 这是什么：发送当前视频的一条弹幕。
    // 为什么能实现：把 videoId、seconds、text 和当前用户信息组装成 JSON，POST 到 /videos/barrages。
    // 什么时候调用：用户在播放页输入弹幕并点击发送或按回车时调用。
    // 和谁配合：PlayerPage 负责输入校验和成功后的立即显示。
    void sendBarrage(const QString &videoId, int seconds, const QString &text);

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

    // 这是什么：查询当前账号对视频的初始点赞状态和点赞数。
    // 为什么能实现：后端按 account + videoId 查询点赞关系，并返回 liked/likeCount。
    // 什么时候调用：播放页打开并拿到 videoId 后调用。
    // 和谁配合：PlayerPage 收到 videoLikeStatusLoaded 后初始化按钮和数量。
    void fetchVideoLikeStatus(const QString &videoId);

    // 这是什么：请求当前视频的上次播放进度。
    // 为什么能实现：播放页已经持有 videoId，ApiClient 可带上当前账号请求 /videos/watch-progress。
    // 什么时候调用：PlayerPage 打开后，需要知道是否要从上次看到的位置继续播放时调用。
    // 和谁配合：PlayerPage 收到 watchProgressLoaded 后，在 mpv 准备好时 seek 到对应秒数。
    void fetchWatchProgress(const QString &videoId);

    // 这是什么：保存当前视频的播放进度。
    // 为什么能实现：PlayerPage 持续维护当前播放秒数，ApiClient 把 videoId/account/seconds POST 给后端。
    // 什么时候调用：播放过程中定时保存，或播放页关闭/隐藏时保存一次。
    // 和谁配合：mock/后端保存记录，下次 fetchWatchProgress() 再读回来。
    void saveWatchProgress(const QString &videoId, int seconds);

    // 这是什么：请求当前视频的评论列表。
    // 为什么能实现：把 videoId 放进 GET 查询参数，后端即可返回该视频对应的评论数组。
    // 什么时候调用：用户在播放页打开评论窗口时调用。
    // 和谁配合：CommentDialog 显示加载状态，commentsLoaded 返回解析后的评论列表。
    void fetchComments(const QString &videoId);

    // 这是什么：发表当前登录用户的一条视频评论。
    // 为什么能实现：从 DataCenter 读取用户身份，再把 videoId 和正文作为 JSON POST 给后端。
    // 什么时候调用：评论窗口输入合法内容并点击发送时调用。
    // 和谁配合：CommentDialog 收集正文，commentSent 返回后立即把新评论插到列表顶部。
    void sendComment(const QString &videoId, const QString &content);

    // 这是什么：按关键词请求视频搜索结果。
    // 为什么能实现：把关键词作为 GET 查询参数发送，后端可在标题、作者、分类、标签和简介中匹配。
    // 什么时候调用：用户在首页输入非空关键词并点击搜索或按回车时调用。
    // 和谁配合：player.cpp 负责输入和渲染，searchResultsLoaded 返回可为空的视频列表。
    void searchVideos(const QString &keyword);

    // 这是什么：查询当前用户是否已收藏某个视频。
    // 为什么能实现：把 videoId 和 DataCenter 当前账号作为 GET 参数，后端可查询二者的收藏关系。
    // 什么时候调用：播放页打开并拿到 videoId 后调用。
    // 和谁配合：PlayerPage 收到 videoFavoriteStatusLoaded 后初始化收藏按钮。
    void fetchFavoriteStatus(const QString &videoId);

    // 这是什么：收藏当前视频。
    // 为什么能实现：把 videoId/account POST 到收藏接口，后端写入用户和视频的关系。
    // 什么时候调用：已登录用户点击未收藏状态的按钮时调用。
    // 和谁配合：PlayerPage 等待 videoFavoriteChanged 后更新按钮。
    void favoriteVideo(const QString &videoId);

    // 这是什么：取消收藏当前视频。
    // 为什么能实现：请求体与收藏相同，但后端从收藏关系中删除当前账号。
    // 什么时候调用：已登录用户点击已收藏状态的按钮时调用。
    // 和谁配合：PlayerPage 等待 videoFavoriteChanged 后更新按钮。
    void unfavoriteVideo(const QString &videoId);

    // 这是什么：请求当前登录用户收藏的视频列表。
    // 为什么能实现：ApiClient 从 DataCenter 读取账号，后端按账号返回对应 VideoInfo 数组。
    // 什么时候调用：用户点击“我的收藏”入口时调用。
    // 和谁配合：player.cpp 收到 favoriteVideosLoaded 后在“我的”页面渲染 VideoBox。
    void fetchFavoriteVideos();

    // 这是什么：读取当前登录用户的个人资料。
    // 为什么能实现：从 DataCenter 读取账号作为 GET 参数，后端返回昵称和简介。
    // 什么时候调用：登录成功后或进入“我的”页面需要刷新资料时调用。
    // 和谁配合：userProfileLoaded 把 UserInfo 交给 player.cpp 和 DataCenter。
    void fetchUserProfile();

    // 这是什么：修改当前登录用户的昵称和简介。
    // 为什么能实现：把 account/userName/description 作为 JSON POST，后端校验并返回最终资料。
    // 什么时候调用：ProfileDialog 校验输入并点击保存时调用。
    // 和谁配合：userProfileUpdated 通知 player.cpp 刷新“我的”页面。
    void updateUserProfile(const QString &userName, const QString &description);

    // 这是什么：请求当前登录用户发布的视频列表。
    // 为什么能实现：DataCenter 提供账号，后端按视频 ownerAccount 字段筛选。
    // 什么时候调用：用户点击“我的视频”或上传成功返回“我的”页面时调用。
    // 和谁配合：myVideosLoaded 把标准 VideoInfo 列表交给 player.cpp 渲染。
    void fetchMyVideos();

    // 这是什么：上传当前用户选择的真实头像文件。
    // 为什么能实现：multipart 同时发送账号和图片二进制，后端保存后返回 avatarPath。
    // 什么时候调用：“我的”页面选择并校验头像图片后调用。
    // 和谁配合：avatarUploaded 更新 DataCenter 和头像按钮，个人资料接口负责后续恢复。
    void uploadAvatar(const QString &filePath);

    // 这是什么：读取后台视频审核列表；为什么能实现：GET 返回结构化审核数组。
    // 什么时候调用：后台初始化或审核成功后；和谁配合：AdminWidget 表格。
    void fetchAdminReviews();
    // 这是什么：提交通过/拒绝审核；为什么能实现：videoId 定位记录，status 表达结果。
    // 什么时候调用：审核操作按钮点击后；和谁配合：mock 审核状态和列表刷新。
    void reviewVideo(const QString &videoId, const QString &status);
    // 这是什么：读取后台角色用户列表；为什么能实现：GET 返回账号角色数组。
    // 什么时候调用：后台初始化或角色操作成功后；和谁配合：AdminWidget 角色表格。
    void fetchAdminUsers();
    // 这是什么：修改用户角色/状态或删除；为什么能实现：account + action 描述操作。
    // 什么时候调用：角色操作按钮点击后；和谁配合：mock 角色状态和列表刷新。
    void updateAdminUser(const QString &account, const QString &action);

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

    // 这是什么：邮箱验证码申请成功通知。
    // 为什么能实现：后端返回 authcodeId，mock 环境同时返回 debugCode 方便手动联调。
    // 什么时候触发：POST /login/email-code 成功后触发。
    // 和谁配合：Login 保存会话 id、恢复按钮并向用户展示测试验证码。
    void emailCodeSent(const QString &authcodeId, const QString &debugCode);

    // 这是什么：邮箱验证码申请失败通知。
    // 为什么能实现：格式错误、网络错误和后端拒绝统一转换成 message。
    // 什么时候触发：requestEmailCode() 失败时触发。
    // 和谁配合：Login 恢复“获取验证码”按钮并提示原因。
    void emailCodeFailed(const QString &message);

    // 这是什么：退出登录成功/失败通知；为什么能实现：POST 响应提供 success/message。
    // 什么时候触发：logout() 完成后；和谁配合：player.cpp 清空状态或提示错误。
    void logoutSucceeded();
    void logoutFailed(const QString &message);

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

    // 这是什么：真实文件上传进度通知。
    // 为什么能实现：QNetworkReply::uploadProgress 会持续给出已发送和总字节数，可换算为百分比。
    // 什么时候触发：multipart 请求发送过程中触发，范围为 0 到 100。
    // 和谁配合：UploadVideoPage 更新进度文字和发布按钮。
    void uploadProgressChanged(int percent);

    // 这是什么：播放地址接口成功后的通知信号。
    // 为什么能实现：GET /videos/play-url 返回 success=true 和 playUrl 后，ApiClient 可以把地址交给播放页。
    // 什么时候触发：fetchPlayUrl(videoId) 收到非空 playUrl 后触发。
    // 和谁配合：PlayerPage 用 playUrl 启动 MpvPlayer 播放。
    void playUrlLoaded(const QString &playUrl);

    // 这是什么：播放地址接口失败后的通知信号。
    // 为什么能实现：网络错误、JSON 异常、success=false 或 playUrl 为空都会转成失败消息。
    // 什么时候触发：fetchPlayUrl(videoId) 无法拿到可播放地址时触发。
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

    // 这是什么：视频初始点赞状态加载成功后的通知。
    // 为什么能实现：状态接口返回 liked 和 likeCount，页面可直接采用最终值。
    // 什么时候触发：fetchVideoLikeStatus() 成功后触发。
    // 和谁配合：PlayerPage 初始化 m_isLiked、点赞按钮和数量。
    void videoLikeStatusLoaded(bool liked, const QString &likeCount);

    // 这是什么：点赞接口失败后的通知信号。
    // 为什么能实现：网络错误、缺少 videoId 或后端返回失败都会统一转成 message。
    // 什么时候触发：点赞或取消点赞请求无法完成时触发。
    // 和谁配合：PlayerPage 记录日志，并保留当前点赞状态不变。
    void videoLikeFailed(const QString &message);

    // 这是什么：播放记录加载成功后的通知信号。
    // 为什么能实现：/videos/watch-progress 返回 seconds，ApiClient 解析后可把整数秒交给播放页。
    // 什么时候触发：fetchWatchProgress() 成功拿到有效 seconds 后触发。
    // 和谁配合：PlayerPage 缓存这个秒数，并在播放地址加载完成后调用 mpv seek。
    void watchProgressLoaded(int seconds);

    // 这是什么：播放记录保存成功后的通知信号。
    // 为什么能实现：POST /videos/watch-progress 返回 success=true 时说明 mock/后端已保存。
    // 什么时候触发：saveWatchProgress() 成功提交当前秒数后触发。
    // 和谁配合：PlayerPage 目前只记录日志，不打断播放。
    void watchProgressSaved();

    // 这是什么：播放记录接口失败后的通知信号。
    // 为什么能实现：网络错误、缺少 videoId 或秒数非法都会统一转成 message。
    // 什么时候触发：加载或保存播放记录失败时触发。
    // 和谁配合：PlayerPage 记录日志，并继续正常播放。
    void watchProgressFailed(const QString &message);

    // 这是什么：评论列表加载成功后的通知信号。
    // 为什么能实现：Qt 信号槽可以把异步解析完成的 QList<CommentInfo> 交回界面。
    // 什么时候触发：GET /videos/comments 返回 success=true 和有效评论数组后触发。
    // 和谁配合：PlayerPage 转交给 CommentDialog 刷新列表。
    void commentsLoaded(const QList<CommentInfo> &comments);

    // 这是什么：发表评论成功后的通知信号。
    // 为什么能实现：后端返回最终评论对象，页面无需自行拼接 id 和时间。
    // 什么时候触发：POST /videos/comments 成功保存评论后触发。
    // 和谁配合：CommentDialog 把新评论插入顶部并恢复发送按钮。
    void commentSent(const CommentInfo &comment);

    // 这是什么：评论读取或发送失败后的统一通知信号。
    // 为什么能实现：网络错误、参数错误和后端拒绝都能归一成 message。
    // 什么时候触发：fetchComments() 或 sendComment() 无法完成时触发。
    // 和谁配合：CommentDialog 展示错误并恢复可操作状态，视频播放不受影响。
    void commentRequestFailed(const QString &message);

    // 这是什么：视频搜索成功后的结果信号。
    // 为什么能实现：ApiClient 把后端 JSON 数组转成 QList<VideoInfo>，页面可复用现有 VideoBox 渲染流程。
    // 什么时候触发：GET /videos/search 返回 success=true 时触发，零条结果也属于成功。
    // 和谁配合：player.cpp 更新 m_homeVideos，并继续应用当前分类和标签筛选。
    void searchResultsLoaded(const QList<VideoInfo> &videos);

    // 这是什么：视频搜索失败后的通知信号。
    // 为什么能实现：网络错误、空关键词和后端业务错误都会转成统一 message。
    // 什么时候触发：searchVideos() 无法得到有效响应时触发。
    // 和谁配合：player.cpp 恢复搜索按钮并保留搜索前的视频列表。
    void searchFailed(const QString &message);

    // 这是什么：当前视频收藏状态查询成功后的通知。
    // 为什么能实现：后端返回 favorited 布尔值，页面可以直接初始化按钮状态。
    // 什么时候触发：fetchFavoriteStatus() 成功时触发。
    // 和谁配合：PlayerPage 更新 m_isFavorited 和收藏按钮。
    void videoFavoriteStatusLoaded(bool favorited);

    // 这是什么：收藏关系修改成功后的通知。
    // 为什么能实现：后端返回最终 favorited 状态，避免页面提前猜测请求结果。
    // 什么时候触发：favoriteVideo() 或 unfavoriteVideo() 成功时触发。
    // 和谁配合：PlayerPage 根据最终状态更新星标按钮。
    void videoFavoriteChanged(bool favorited);

    // 这是什么：我的收藏视频列表加载成功后的通知。
    // 为什么能实现：接口视频字段与 VideoInfo 一致，可直接解析成首页同款列表。
    // 什么时候触发：fetchFavoriteVideos() 成功时触发，空列表也属于成功。
    // 和谁配合：player.cpp 在“我的收藏”区域展示 VideoBox。
    void favoriteVideosLoaded(const QList<VideoInfo> &videos);

    // 这是什么：收藏业务失败后的统一通知。
    // 为什么能实现：未登录、参数错误、网络失败和后端拒绝都转换成 message。
    // 什么时候触发：收藏状态、收藏操作或收藏列表请求失败时触发。
    // 和谁配合：PlayerPage/player.cpp 只提示或记录错误，不影响其它功能。
    void favoriteRequestFailed(const QString &message);

    // 这是什么：个人资料读取成功后的通知。
    // 为什么能实现：接口响应字段可以直接转成 UserInfo。
    // 什么时候触发：GET /users/profile 成功时触发。
    // 和谁配合：player.cpp 写入 DataCenter 并刷新页面。
    void userProfileLoaded(const UserInfo &user);

    // 这是什么：个人资料修改成功后的通知。
    // 为什么能实现：后端返回最终保存结果，页面以它为准更新状态。
    // 什么时候触发：POST /users/profile 成功时触发。
    // 和谁配合：ProfileDialog 恢复按钮，player.cpp 更新展示。
    void userProfileUpdated(const UserInfo &user);

    // 这是什么：个人资料请求失败后的通知。
    // 为什么能实现：未登录、校验错误和网络错误统一转换成 message。
    // 什么时候触发：读取或修改资料失败时触发。
    // 和谁配合：player.cpp/ProfileDialog 展示错误并保留原资料。
    void userProfileFailed(const QString &message);

    // 这是什么：当前用户作品列表加载成功后的通知。
    // 为什么能实现：我的视频接口沿用 VideoInfo 字段，页面可复用 VideoBox。
    // 什么时候触发：GET /users/videos 成功时触发，空列表也属于成功。
    // 和谁配合：player.cpp 调用 renderMyVideoList() 展示“我的作品”。
    void myVideosLoaded(const QList<VideoInfo> &videos);

    // 这是什么：我的视频列表加载失败后的通知。
    // 为什么能实现：未登录、网络错误和业务失败统一转换成 message。
    // 什么时候触发：fetchMyVideos() 无法完成时触发。
    // 和谁配合：player.cpp 在作品区域展示错误但不影响其它页面。
    void myVideosFailed(const QString &message);

    // 这是什么：头像文件上传成功后的通知。
    // 为什么能实现：后端返回实际保存路径，客户端可以立即读取并显示。
    // 什么时候触发：POST /users/avatar 成功后触发。
    // 和谁配合：player.cpp 更新 UserInfo.avatarPath 和头像按钮。
    void avatarUploaded(const QString &avatarPath);

    // 这是什么：头像上传失败后的通知。
    // 为什么能实现：本地文件错误、网络错误和后端校验错误统一转成 message。
    // 什么时候触发：uploadAvatar() 无法完成时触发。
    // 和谁配合：player.cpp 提示错误并保留原头像。
    void avatarUploadFailed(const QString &message);

    // 这是什么：审核列表/角色列表成功信号；为什么能实现：异步结果转为固定结构体列表。
    // 什么时候触发：对应 GET 成功后；和谁配合：AdminWidget 更新数据源。
    void adminReviewsLoaded(const QList<AdminReviewInfo> &reviews);
    void adminUsersLoaded(const QList<AdminUserInfo> &users);
    // 这是什么：后台写操作成功/失败信号；为什么能实现：统一传递刷新目标或错误原因。
    // 什么时候触发：审核或角色 POST 完成后；和谁配合：AdminWidget 刷新或提示。
    void adminActionSucceeded(const QString &target);
    void adminRequestFailed(const QString &message);

private:
    // 这是什么：点赞和取消点赞共用的 POST 请求实现。
    // 为什么这样做：两个接口请求体和响应解析几乎一样，集中到一个函数可以减少重复和不一致。
    // 什么时候调用：likeVideo() 和 unlikeVideo() 分别传入不同 URL 后调用。
    // 和谁配合：m_likeUrl/m_unlikeUrl 提供接口地址，videoLikeChanged/videoLikeFailed 通知播放页。
    void sendVideoLikeRequest(const QUrl &url, const QString &videoId);

    // 这是什么：收藏和取消收藏共用的 POST 请求实现。
    // 为什么这样做：两个操作只有 URL 不同，请求体和响应解析完全相同，集中处理可避免重复。
    // 什么时候调用：favoriteVideo() 和 unfavoriteVideo() 分别传入对应地址时调用。
    // 和谁配合：videoFavoriteChanged 把后端最终状态交回 PlayerPage。
    void sendVideoFavoriteRequest(const QUrl &url, const QString &videoId);

    // 这是什么：当前首页视频列表接口地址。
    // 为什么这样做：默认指向真实后端，也可通过 VIDEO_API_BASE_URL 切换到 mock 或其他环境。
    // 什么时候使用：fetchVideos() 创建 QNetworkRequest 时使用。
    // 和谁配合：tools/mock_videos_server.py 当前提供同路径的 /videos 响应。
    QUrl m_videosUrl = QUrl("http://192.168.19.129:9000/videos");

    // 这是什么：当前临时登录接口地址。
    // 为什么这样做：默认指向真实后端，也可通过 VIDEO_API_BASE_URL 切换到 mock 或其他环境。
    // 什么时候使用：login() 创建 POST /login 请求时使用。
    // 和谁配合：tools/mock_videos_server.py 提供同路径的临时登录响应。
    QUrl m_loginUrl = QUrl("http://192.168.19.129:9000/login");
    QUrl m_emailCodeUrl = QUrl("http://192.168.19.129:9000/login/email-code");
    QUrl m_emailLoginUrl = QUrl("http://192.168.19.129:9000/login/email");
    // 这是什么：退出登录接口地址；为什么这样做：将会话结束表达为明确 POST 动作。
    // 什么时候使用：logout() 创建请求时；和谁配合：mock/真实后端会话管理。
    QUrl m_logoutUrl = QUrl("http://192.168.19.129:9000/logout");

    // 这是什么：当前上传视频元数据接口地址。
    // 为什么这样做：第一版沿用 REST 风格，GET /videos 获取列表，POST /videos 发布新视频元数据。
    // 什么时候使用：uploadVideo() 创建 POST /videos 请求时使用。
    // 和谁配合：tools/mock_videos_server.py 处理同路径的上传请求。
    QUrl m_uploadVideoUrl = QUrl("http://192.168.19.129:9000/videos");

    // 这是什么：真实视频文件上传接口地址。
    // 为什么这样做：multipart 请求与旧的纯 JSON 元数据请求分开，后端可明确解析二进制文件。
    // 什么时候使用：uploadVideo() 校验本地文件可读后使用。
    // 和谁配合：UploadVideoPage 提供路径，mock server 保存 videoFile/coverFile 部分。
    QUrl m_uploadVideoFilesUrl = QUrl("http://192.168.19.129:9000/videos/upload");

    // 这是什么：当前最小版播放地址接口地址。
    // 为什么这样做：先不引入 videoId，固定接口能最快验证播放页从网络拿播放地址。
    // 什么时候使用：fetchPlayUrl() 创建 GET /videos/play-url 请求时使用。
    // 和谁配合：tools/mock_videos_server.py 返回本地 test.mp4 路径。
    QUrl m_playUrlUrl = QUrl("http://192.168.19.129:9000/videos/play-url");

    // 这是什么：当前弹幕接口地址。
    // 为什么这样做：第一版沿用固定路径，GET 拉取弹幕，POST 发送弹幕。
    // 什么时候使用：fetchBarrages() 和 sendBarrage() 创建请求时使用。
    // 和谁配合：tools/mock_videos_server.py 的 /videos/barrages 内存接口。
    QUrl m_barragesUrl = QUrl("http://192.168.19.129:9000/videos/barrages");

    // 这是什么：当前视频详情接口地址。
    // 为什么这样做：第一版用固定 /videos/detail 路径配合 id query，后续接真实后端时容易升级为按视频 id 查详情。
    // 什么时候使用：fetchVideoDetail() 创建 GET /videos/detail?id=... 请求时使用。
    // 和谁配合：tools/mock_videos_server.py 根据 id 返回 mock 视频详情。
    QUrl m_videoDetailUrl = QUrl("http://192.168.19.129:9000/videos/detail");

    // 这是什么：当前点赞接口地址。
    // 为什么这样做：第一版用 POST /videos/like 表达“当前用户点赞当前视频”的动作。
    // 什么时候使用：likeVideo() 创建请求时使用。
    // 和谁配合：tools/mock_videos_server.py 在内存中记录用户点赞状态。
    QUrl m_likeUrl = QUrl("http://192.168.19.129:9000/videos/like");

    // 这是什么：当前取消点赞接口地址。
    // 为什么这样做：第一版用 POST /videos/unlike 表达“当前用户取消点赞当前视频”的动作。
    // 什么时候使用：unlikeVideo() 创建请求时使用。
    // 和谁配合：tools/mock_videos_server.py 在内存中移除用户点赞状态。
    QUrl m_unlikeUrl = QUrl("http://192.168.19.129:9000/videos/unlike");

    // 这是什么：当前用户对视频点赞状态的查询地址。
    // 为什么这样做：查询关系是读取操作，和点赞/取消点赞 POST 分开。
    // 什么时候使用：fetchVideoLikeStatus() 创建 GET 请求时使用。
    // 和谁配合：mock server 的 GET /videos/like-status。
    QUrl m_likeStatusUrl = QUrl("http://192.168.19.129:9000/videos/like-status");

    // 这是什么：当前播放记录接口地址。
    // 为什么这样做：第一版 GET/POST 共用 /videos/watch-progress，分别负责读取和保存进度。
    // 什么时候使用：fetchWatchProgress() 和 saveWatchProgress() 创建请求时使用。
    // 和谁配合：tools/mock_videos_server.py 用内存保存 account + videoId 对应的秒数。
    QUrl m_watchProgressUrl = QUrl("http://192.168.19.129:9000/videos/watch-progress");

    // 这是什么：当前评论列表和发表评论共用的接口地址。
    // 为什么这样做：GET 表示读取集合，POST 表示向集合新增评论，符合当前项目的 REST 风格。
    // 什么时候使用：fetchComments() 和 sendComment() 创建网络请求时使用。
    // 和谁配合：tools/mock_videos_server.py 提供同路径的内存评论接口。
    QUrl m_commentsUrl = QUrl("http://192.168.19.129:9000/videos/comments");

    // 这是什么：当前视频搜索接口地址。
    // 为什么这样做：搜索是读取操作，使用 GET 并通过 keyword 查询参数表达条件。
    // 什么时候使用：searchVideos() 创建网络请求时使用。
    // 和谁配合：tools/mock_videos_server.py 的 GET /videos/search。
    QUrl m_searchUrl = QUrl("http://192.168.19.129:9000/videos/search");

    // 这是什么：收藏状态、收藏操作和个人收藏列表的接口地址。
    // 为什么这样做：读状态、写关系、读列表职责不同，使用独立 REST 路径更清晰。
    // 什么时候使用：播放页初始化/点击收藏，以及“我的收藏”入口请求列表时使用。
    // 和谁配合：mock server 的 VIDEO_FAVORITES 关系集合。
    QUrl m_favoriteStatusUrl = QUrl("http://192.168.19.129:9000/videos/favorite-status");
    QUrl m_favoriteUrl = QUrl("http://192.168.19.129:9000/videos/favorite");
    QUrl m_unfavoriteUrl = QUrl("http://192.168.19.129:9000/videos/unfavorite");
    QUrl m_favoriteVideosUrl = QUrl("http://192.168.19.129:9000/users/favorites");

    // 这是什么：个人资料读取和修改共用地址。
    // 为什么这样做：GET 负责读取，POST 负责更新同一个用户资源。
    // 什么时候使用：fetchUserProfile() 和 updateUserProfile() 发请求时使用。
    // 和谁配合：mock server 的 USERS 内存数据。
    QUrl m_userProfileUrl = QUrl("http://192.168.19.129:9000/users/profile");

    // 这是什么：当前用户发布视频列表接口地址。
    // 为什么这样做：用户资源下的视频集合使用独立 GET 路径表达归属关系。
    // 什么时候使用：fetchMyVideos() 创建请求时使用。
    // 和谁配合：mock server 根据 ownerAccount 返回 VIDEOS 子集。
    QUrl m_myVideosUrl = QUrl("http://192.168.19.129:9000/users/videos");
    QUrl m_avatarUploadUrl = QUrl("http://192.168.19.129:9000/users/avatar");
    QUrl m_adminReviewsUrl = QUrl("http://192.168.19.129:9000/admin/reviews");
    QUrl m_adminReviewActionUrl = QUrl("http://192.168.19.129:9000/admin/reviews/action");
    QUrl m_adminUsersUrl = QUrl("http://192.168.19.129:9000/admin/users");
    QUrl m_adminUserActionUrl = QUrl("http://192.168.19.129:9000/admin/users/action");

    // 这是什么：Qt 网络请求管理器。
    // 为什么能实现：它负责创建并发送 GET/POST 等请求，返回 QNetworkReply 表示异步响应。
    // 什么时候使用：每次 fetchVideos() 发请求时使用。
    // 和谁配合：QNetworkRequest 描述接口地址，QNetworkReply 承载后端响应。
    QNetworkAccessManager *m_networkManager = nullptr;
};

#endif // APICLIENT_H
