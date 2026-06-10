#include "datacenter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

DataCenter &DataCenter::instance()
{
    static DataCenter dataCenter;
    return dataCenter;
}

QList<VideoInfo> parseVideosFromJson(const QByteArray &data)
{
    // 这是什么：把接口响应体里的 JSON 数组转换成 QList<VideoInfo>。
    // 为什么能实现：QJsonDocument 先把 QByteArray 解析成 JSON 文档，再按每个对象的字段填充 VideoInfo。
    // 什么时候调用：ApiClient 收到 /videos 响应并且网络没有报错后调用。
    // 和谁配合：ApiClient 负责发请求和读响应，player/DataCenter 负责保存和展示解析结果。
    QList<VideoInfo> videos;

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    const QJsonArray array = doc.array();

    for (const QJsonValue &value : array) {
        const QJsonObject obj = value.toObject();

        VideoInfo video;
        video.title = obj["title"].toString();
        video.userName = obj["userName"].toString();
        video.date = obj["date"].toString();
        video.duration = obj["duration"].toString();
        video.playCount = obj["playCount"].toString();
        video.likeCount = obj["likeCount"].toString();
        video.category = obj["category"].toString();

        const QJsonArray tagArray = obj["tags"].toArray();
        for (const QJsonValue &tagValue : tagArray) {
            video.tags.append(tagValue.toString());
        }

        videos.append(video);
    }

    return videos;
}

DataCenter::DataCenter()
{
    m_categories = {"历史", "美食", "游戏", "科技", "运动", "动物", "旅游", "电影"};

    m_categoryTags.insert("历史", {"中国史", "世界史", "人物传记", "历史故事", "文化遗产"});
    m_categoryTags.insert("美食", {"美食测评", "美食制作", "地方小吃", "家常菜", "探店"});
    m_categoryTags.insert("游戏", {"游戏攻略", "实况解说", "新手教程", "赛事集锦", "主机游戏"});
    m_categoryTags.insert("科技", {"数码评测", "前沿科技", "编程开发", "人工智能", "软件工具"});
    m_categoryTags.insert("运动", {"健身训练", "篮球", "足球", "跑步", "户外运动"});
    m_categoryTags.insert("动物", {"萌宠日常", "动物世界", "养宠知识", "救助记录", "自然观察"});
    m_categoryTags.insert("旅游", {"北京旅游", "城市漫步", "旅行攻略", "风景记录", "酒店体验"});
    m_categoryTags.insert("电影", {"电影解说", "影评", "预告解析", "幕后故事", "经典片段"});

    m_homeVideos = {
        {"【北京旅游攻略】一条视频告诉你去了北京该怎么玩", "用户昵称", "9-16", "25:52", "26.1万", "1226", "旅游", {"北京旅游", "旅行攻略"}},
        {"一条视频告诉你去了北京该怎么玩", "用户昵称", "9-16", "25:52", "26.1万", "1226", "旅游", {"城市漫步", "风景记录"}},
        {"世界史入门：从文明起源讲到现代", "用户昵称", "9-16", "18:36", "18.8万", "935", "历史", {"世界史", "历史故事"}},
        {"美食测评：北京胡同里的宝藏小店", "用户昵称", "9-16", "12:08", "9.7万", "521", "美食", {"美食测评", "探店"}},
        {"游戏攻略：新手也能快速上手的通关路线", "用户昵称", "9-16", "21:47", "32.4万", "2048", "游戏", {"游戏攻略", "新手教程"}},
        {"科技观察：一分钟看懂智能设备新趋势", "用户昵称", "9-16", "08:45", "7.2万", "318", "科技", {"前沿科技", "数码评测"}},
        {"运动训练：每天十分钟改善体态", "用户昵称", "9-16", "16:20", "11.3万", "746", "运动", {"健身训练", "跑步"}},
        {"动物世界：森林里的奇妙一天", "用户昵称", "9-16", "14:33", "15.6万", "889", "动物", {"动物世界", "自然观察"}},
    };
}

QStringList DataCenter::categories() const
{
    return m_categories;
}

QStringList DataCenter::tagsForCategory(const QString &category) const
{
    return m_categoryTags.value(category);
}

QStringList DataCenter::allTags() const
{
    QStringList tags;
    for (const QString &category : m_categories) {
        for (const QString &tag : m_categoryTags.value(category)) {
            if (!tags.contains(tag)) {
                tags.append(tag);
            }
        }
    }
    return tags;
}

QList<VideoInfo> DataCenter::homeVideos() const
{
    // 这是什么：返回当前首页使用的视频数据。
    // 为什么能实现：m_homeVideos 始终保存首页当前数据源，默认是假数据，接口成功后会变成响应数据。
    // 什么时候调用：首页初始化、接口数据刷新后、分类和标签筛选重新渲染时调用。
    // 和谁配合：player.cpp 把返回结果放入页面缓存 m_homeVideos，再交给 renderHomeVideos() 生成 VideoBox。
    return m_homeVideos;
}

void DataCenter::setHomeVideos(const QList<VideoInfo> &videos)
{
    // 这是什么：把接口返回的视频列表写进 DataCenter。
    // 为什么能实现：ApiClient 已经完成 JSON 到 VideoInfo 的转换，这里只负责保存当前可展示的数据源。
    // 什么时候调用：首页收到 ApiClient::videosLoaded 信号并确认列表非空后调用。
    // 和谁配合：homeVideos() 随后把最新数据交回 player.cpp，用来刷新首页卡片和筛选结果。
    if (videos.isEmpty()) {
        return;
    }

    m_homeVideos = videos;
}

void DataCenter::addBarrage(const QString &videoKey, int seconds, const QString &text)
{
    const QString trimmedText = text.trimmed();
    if (videoKey.isEmpty() || seconds < 0 || trimmedText.isEmpty()) {
        return;
    }

    m_barragesByVideo[videoKey][seconds].append(trimmedText.left(30));
}

QStringList DataCenter::barragesAt(const QString &videoKey, int seconds) const
{
    if (videoKey.isEmpty() || seconds < 0) {
        return {};
    }

    return m_barragesByVideo.value(videoKey).value(seconds);
}

void DataCenter::clearBarrages(const QString &videoKey)
{
    if (!videoKey.isEmpty()) {
        m_barragesByVideo.remove(videoKey);
    }
}
