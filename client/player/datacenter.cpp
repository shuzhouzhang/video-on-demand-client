#include "datacenter.h"

DataCenter &DataCenter::instance()
{
    static DataCenter dataCenter;
    return dataCenter;
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
