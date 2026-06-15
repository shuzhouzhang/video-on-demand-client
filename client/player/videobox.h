// videobox.h 声明首页视频卡片组件。
// 一个 VideoBox 只负责展示一个视频的封面、标题、数据和用户信息。
#ifndef VIDEOBOX_H
#define VIDEOBOX_H

#include <QWidget>

namespace Ui {
class VideoBox;
}

class VideoBox : public QWidget
{
    Q_OBJECT

public:
    explicit VideoBox(QWidget *parent = nullptr);
    ~VideoBox() override;

    // 第一版使用静态展示数据，后续接真实接口时可替换成 VideoInfo 模型。
    void setVideoInfo(const QString &id,
                      const QString &title,
                      const QString &userName,
                      const QString &date,
                      const QString &duration,
                      const QString &playCount,
                      const QString &likeCount);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    // VideoBox 不直接创建播放页，只把点击事件和视频数据发给外层页面处理。
    void videoClicked(const QString &id,
                      const QString &title,
                      const QString &userName,
                      const QString &date,
                      const QString &duration,
                      const QString &playCount,
                      const QString &likeCount);

private:
    Ui::VideoBox *ui;
    QString m_id;
    QString m_title;
    QString m_userName;
    QString m_date;
    QString m_duration;
    QString m_playCount;
    QString m_likeCount;
};

#endif // VIDEOBOX_H
