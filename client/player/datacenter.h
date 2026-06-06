#ifndef DATACENTER_H
#define DATACENTER_H

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

class DataCenter
{
public:
    static DataCenter &instance();

    QStringList categories() const;
    QStringList tagsForCategory(const QString &category) const;
    QStringList allTags() const;
    QList<VideoInfo> homeVideos() const;

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
