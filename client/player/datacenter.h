#ifndef DATACENTER_H
#define DATACENTER_H

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

// 这是什么：把后端 /videos 返回的 JSON 响应解析成首页能使用的视频列表。
// 为什么能实现：VideoInfo 字段和 mock/后端 JSON 字段一一对应，解析后页面不用再关心原始 JSON。
// 什么时候调用：ApiClient 收到 QNetworkReply 响应体并确认网络请求成功后调用。
// 和谁配合：ApiClient 负责拿到响应数据，DataCenter/player 负责保存和展示解析后的视频列表。
QList<VideoInfo> parseVideosFromJson(const QByteArray &data);

class DataCenter
{
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
    // 什么时候调用：ApiClient::videosLoaded 触发后，player::setHomeVideos() 接收到非空列表时调用。
    // 和谁配合：homeVideos() 负责把保存后的数据再交给首页读取和展示。
    void setHomeVideos(const QList<VideoInfo> &videos);

    // 这是什么：保存当前登录用户信息。
    // 为什么能实现：登录成功后已经拿到 userName/account，DataCenter 只负责内存保存这份当前状态。
    // 什么时候调用：Login::loginSuccess 触发后，player::updateLoginState() 接收到用户信息时调用。
    // 和谁配合：currentUser() 和 isLoggedIn() 让页面后续都从同一个地方读取登录状态。
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
    // 为什么能实现：临时登录阶段只要 currentUser 的账号不为空，就认为已有有效登录状态。
    // 什么时候调用：点击头像、资料、作品、关注、设置等需要登录的入口前调用。
    // 和谁配合：player.cpp 用它决定是打开登录窗口还是继续执行当前操作。
    bool isLoggedIn() const;

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
    QHash<QString, QHash<int, QStringList>> m_barragesByVideo;
};

#endif // DATACENTER_H
