#ifndef DATACENTER_H
#define DATACENTER_H

#include <QObject>
#include <QUrl>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

struct VideoInfo {
    QString id;
    QString title;
    QString userName;
    QString date;
    QString duration;
    QString playCount;
    QString likeCount;
    QString category;
    QStringList tags;
    QString description;
};

// 这是什么：当前登录用户的轻量信息。
// 为什么这样做：只保存页面当前需要展示和判断登录状态的字段，避免提前引入 token、权限等复杂状态。
// 什么时候使用：登录接口或邮箱模拟登录成功后写入，页面需要判断当前用户时读取。
// 和谁配合：Login 发出登录成功信号，player.cpp 写入 DataCenter 并刷新“我的”页面。
struct UserInfo {
    QString userName;
    QString account;
    QString description;
    QString avatarPath;
};

// 这是什么：一条视频评论在客户端中的结构化数据。
// 为什么这样做：把接口 JSON 翻译成固定字段后，评论窗口不需要直接处理原始 JSON。
// 什么时候使用：ApiClient 拉取或发送评论成功后创建，CommentDialog 展示评论时读取。
// 和谁配合：mock/后端提供字段，ApiClient 负责解析，CommentDialog 负责渲染。
struct CommentInfo {
    QString id;
    QString videoId;
    QString userName;
    QString account;
    QString content;
    QString createdAt;
};

// 这是什么：后台视频审核列表的一行结构化数据。
// 为什么这样做：ApiClient 解析后，AdminWidget 不需要直接处理 JSON。
// 什么时候使用：后台加载审核列表和执行通过/拒绝操作时使用。
// 和谁配合：mock admin reviews 接口和 AdminWidget 表格。
struct AdminReviewInfo {
    QString videoId;
    QString title;
    QString userId;
    QString status;
    QString uploadTime;
};

// 这是什么：后台角色管理列表的一行用户数据。
// 为什么这样做：固定账号、昵称、角色、状态和创建时间字段，方便表格统一渲染。
// 什么时候使用：后台加载用户列表或修改角色/状态后使用。
// 和谁配合：mock admin users 接口和 AdminWidget 表格。
struct AdminUserInfo {
    QString account;
    QString userName;
    QString role;
    QString status;
    QString createdAt;
};

// 这是什么：把后端 /videos 返回的 JSON 响应解析成首页能使用的视频列表。
// 为什么能实现：VideoInfo 字段和 mock/后端 JSON 字段一一对应，解析后页面不用再关心原始 JSON。
// 什么时候调用：ApiClient 收到 QNetworkReply 响应体并确认网络请求成功后调用。
// 和谁配合：ApiClient 负责拿到响应数据，DataCenter/player 负责保存和展示解析后的视频列表。
QList<VideoInfo> parseVideosFromJson(const QByteArray &data);

class DataCenter : public QObject
{
    Q_OBJECT
public:
    static DataCenter &instance();

    QStringList categories() const;
    QStringList tagsForCategory(const QString &category) const;
    QStringList allTags() const;

    // 这是什么：读取当前首页视频列表。
    // 为什么能实现：DataCenter 内部用 m_homeVideos 保存当前数据源，初始是本地兜底数据，接口成功后会被替换。
    // 什么时候调用：首页初始化、接口刷新后重绘、分类/标签筛选需要遍历视频时调用。
    // 和谁配合：player.cpp 读取它来更新页面缓存 m_homeVideos 并渲染 VideoBox。
    QList<VideoInfo> homeVideos() const;

    // 这是什么：保存接口返回的首页视频列表。
    // 为什么能实现：ApiClient 已经把 JSON 解析成 QList<VideoInfo>，这里直接替换 DataCenter 的当前视频数据。
    // 什么时候调用：ApiClient::videosLoaded 触发后，player::setHomeVideos() 接收到列表时调用。
    // 和谁配合：homeVideos() 负责把保存后的数据再交给首页读取和展示。
    void setHomeVideos(const QList<VideoInfo> &videos);

    // 仅更新展示资料，不创建凭证；登录必须走 saveSession()。
    // 账号发生变化时清掉旧会话，避免账号与 Token 错配。
    void setCurrentUser(const QString &userName, const QString &account);

    // 这是什么：用接口返回的完整资料替换当前用户信息。
    // 为什么能实现：UserInfo 同时包含账号、昵称和简介，可一次更新避免页面读到新旧混合状态。
    // 什么时候调用：个人资料读取或修改接口成功后调用。
    // 和谁配合：ApiClient 提供 UserInfo，player.cpp 随后读取并刷新“我的”页面。
    void setCurrentUser(const UserInfo &user);

    // 这是什么：读取当前登录用户信息。
    // 为什么能实现：setCurrentUser() 会把最近一次登录成功的 userName/account 写入 m_currentUser。
    // 什么时候调用：页面需要刷新昵称、账号或给后续功能拿当前用户时调用。
    // 和谁配合：player.cpp 用它更新“我的”页面展示。
    UserInfo currentUser() const;

    // 这是什么：判断当前是否已经登录。
    // 为什么能实现：同时持有账号和格式有效的 Token 才认为已登录；服务端仍会校验过期时间。
    // 什么时候调用：点击头像、资料、作品、关注、设置等需要登录的入口前调用。
    // 和谁配合：player.cpp 用它决定是打开登录窗口还是继续执行当前操作。
    bool isLoggedIn() const;

    // 会话只保存在单例内存中；所有 ApiClient 共用，且凭证绑定后端来源。
    bool saveSession(const QString &name, const QString &account,
                     const QString &token, const QUrl &origin);
    QString tokenFor(const QUrl &url) const;
    quint64 sessionRevision() const { return m_sessionRevision; }
    void expireSession(quint64 revision);

signals:
    void sessionCleared(bool expired);

public:

    // 这是什么：清空当前登录用户内存状态。
    // 为什么能实现：UserInfo 重置为空后，isLoggedIn() 会立即返回 false。
    // 什么时候调用：主动退出时立即调用，网络注销失败也不保留本地凭证。
    // 和谁配合：player.cpp 恢复游客页面，各业务接口停止携带旧账号。
    void clearCurrentUser();

    void addBarrage(const QString &videoKey, int seconds, const QString &text);
    void setBarrages(const QString &videoKey, const QHash<int, QStringList> &barragesBySecond);
    QStringList barragesAt(const QString &videoKey, int seconds) const;
    void clearBarrages(const QString &videoKey);

private:
    DataCenter();

    QStringList m_categories;
    QHash<QString, QStringList> m_categoryTags;
    QList<VideoInfo> m_homeVideos;
    UserInfo m_currentUser;
    QString m_token;
    QUrl m_sessionOrigin;
    quint64 m_sessionRevision = 0;
    QHash<QString, QHash<int, QStringList>> m_barragesByVideo;
};

#endif // DATACENTER_H
