#ifndef DATACENTER_H
#define DATACENTER_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

struct VideoInfo {
    QString title;
    QString userName;
    QString date;
    QString duration;
    QString playCount;
    QString likeCount;
    QString category;
    QStringList tags;
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

    void addBarrage(const QString &videoKey, int seconds, const QString &text);
    QStringList barragesAt(const QString &videoKey, int seconds) const;
    void clearBarrages(const QString &videoKey);

private:
    DataCenter();

    QStringList m_categories;
    QHash<QString, QStringList> m_categoryTags;
    QList<VideoInfo> m_homeVideos;
    QHash<QString, QHash<int, QStringList>> m_barragesByVideo;
};

#endif // DATACENTER_H
