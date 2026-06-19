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
        video.id = obj["id"].toString();
        video.title = obj["title"].toString();
        video.userName = obj["userName"].toString();
        video.date = obj["date"].toString();
        video.duration = obj["duration"].toString();
        video.playCount = obj["playCount"].toString();
        video.likeCount = obj["likeCount"].toString();
        video.category = obj["category"].toString();
        video.description = obj["description"].toString();

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
        {"local-video-001", "【北京旅游攻略】一条视频告诉你去了北京该怎么玩", "用户昵称", "9-16", "25:52", "26.1万", "1226", "旅游", {"北京旅游", "旅行攻略"}, "北京三日游路线、交通和避坑建议。"},
        {"local-video-002", "一条视频告诉你去了北京该怎么玩", "用户昵称", "9-16", "25:52", "26.1万", "1226", "旅游", {"城市漫步", "风景记录"}, "用城市漫步的方式重新认识北京。"},
        {"local-video-003", "世界史入门：从文明起源讲到现代", "用户昵称", "9-16", "18:36", "18.8万", "935", "历史", {"世界史", "历史故事"}, "适合入门的世界史脉络梳理。"},
        {"local-video-004", "美食测评：北京胡同里的宝藏小店", "用户昵称", "9-16", "12:08", "9.7万", "521", "美食", {"美食测评", "探店"}, "记录一家胡同小店的招牌菜和真实体验。"},
        {"local-video-005", "游戏攻略：新手也能快速上手的通关路线", "用户昵称", "9-16", "21:47", "32.4万", "2048", "游戏", {"游戏攻略", "新手教程"}, "给新手准备的通关路线和操作建议。"},
        {"local-video-006", "科技观察：一分钟看懂智能设备新趋势", "用户昵称", "9-16", "08:45", "7.2万", "318", "科技", {"前沿科技", "数码评测"}, "快速了解智能设备的新功能和新趋势。"},
        {"local-video-007", "运动训练：每天十分钟改善体态", "用户昵称", "9-16", "16:20", "11.3万", "746", "运动", {"健身训练", "跑步"}, "适合日常坚持的十分钟体态训练。"},
        {"local-video-008", "动物世界：森林里的奇妙一天", "用户昵称", "9-16", "14:33", "15.6万", "889", "动物", {"动物世界", "自然观察"}, "跟随镜头观察森林里的自然瞬间。"},
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

void DataCenter::setCurrentUser(const QString &userName, const QString &account)
{
    // 这是什么：把当前登录用户写进 DataCenter。
    // 为什么能实现：登录成功时传入的 userName/account 已经由接口或本地邮箱流程确认可用。
    // 什么时候调用：player::updateLoginState() 收到 Login::loginSuccess 后调用。
    // 和谁配合：currentUser() 和 isLoggedIn() 后续都基于这份状态返回结果。
    m_currentUser.userName = userName.trimmed();
    m_currentUser.account = account.trimmed();
    m_currentUser.description.clear();
    m_currentUser.avatarPath.clear();
}

void DataCenter::setCurrentUser(const UserInfo &user)
{
    // 这是什么：保存接口确认后的完整当前用户资料。
    // 为什么能实现：资料字段在写入前统一去掉首尾空格，DataCenter 继续作为页面共享状态源。
    // 什么时候调用：fetchUserProfile() 或 updateUserProfile() 成功后由 player.cpp 调用。
    // 和谁配合：currentUser() 把最新昵称、账号和简介提供给界面及其它接口。
    m_currentUser.userName = user.userName.trimmed();
    m_currentUser.account = user.account.trimmed();
    m_currentUser.description = user.description.trimmed();
    m_currentUser.avatarPath = user.avatarPath.trimmed();
}

UserInfo DataCenter::currentUser() const
{
    // 这是什么：返回当前登录用户。
    // 为什么能实现：DataCenter 是单例，登录成功写入后，同一进程内其它页面读到的是同一份内存状态。
    // 什么时候调用：“我的”页面刷新用户信息或后续页面需要当前账号时调用。
    // 和谁配合：player.cpp 用返回值更新昵称、账号文本。
    return m_currentUser;
}

bool DataCenter::isLoggedIn() const
{
    // 这是什么：返回当前是否处于登录状态。
    // 为什么能实现：临时登录阶段没有 token，账号非空就是最小可用的登录标记。
    // 什么时候调用：用户点击需要登录的入口前调用。
    // 和谁配合：player.cpp 用它决定是否弹出 Login 窗口。
    return !m_currentUser.account.isEmpty();
}

void DataCenter::clearCurrentUser()
{
    // 这是什么：删除当前进程中的登录用户信息。
    // 为什么能实现：用默认构造 UserInfo 覆盖昵称、账号、简介和头像路径。
    // 什么时候调用：ApiClient::logoutSucceeded 后调用。
    // 和谁配合：player 主窗口随后恢复游客 UI。
    m_currentUser = UserInfo{};
}

void DataCenter::addBarrage(const QString &videoKey, int seconds, const QString &text)
{
    const QString trimmedText = text.trimmed();
    if (videoKey.isEmpty() || seconds < 0 || trimmedText.isEmpty()) {
        return;
    }

    m_barragesByVideo[videoKey][seconds].append(trimmedText.left(30));
}

void DataCenter::setBarrages(const QString &videoKey, const QHash<int, QStringList> &barragesBySecond)
{
    // 这是什么：批量保存某个视频的弹幕列表。
    // 为什么能实现：接口已把弹幕整理成“秒数 -> 文本列表”，这里直接替换该 videoKey 的本地缓存。
    // 什么时候调用：播放页通过 ApiClient 拉取弹幕后，收到 barragesLoaded 信号时调用。
    // 和谁配合：barragesAt() 按播放秒数读取缓存，showBarragesAt() 负责把文本飘到视频区域上。
    if (videoKey.isEmpty()) {
        return;
    }

    m_barragesByVideo[videoKey] = barragesBySecond;
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
