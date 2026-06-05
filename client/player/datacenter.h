#ifndef DATACENTER_H
#define DATACENTER_H

#include <QHash>
#include <QString>
#include <QStringList>

class DataCenter
{
public:
    static DataCenter &instance();

    void addBarrage(const QString &videoKey, int seconds, const QString &text);
    QStringList barragesAt(const QString &videoKey, int seconds) const;
    void clearBarrages(const QString &videoKey);

private:
    DataCenter() = default;

    QHash<QString, QHash<int, QStringList>> m_barragesByVideo;
};

#endif // DATACENTER_H
