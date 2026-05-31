// uploadvideopage.h 声明上传视频页面组件。
// UploadVideoPage 负责承载上传视频的静态表单流程，后续接接口时再替换发布逻辑。
#ifndef UPLOADVIDEOPAGE_H
#define UPLOADVIDEOPAGE_H

#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>

namespace Ui {
class UploadVideoPage;
}

class QPushButton;

class UploadVideoPage : public QWidget
{
    Q_OBJECT

public:
    explicit UploadVideoPage(QWidget *parent = nullptr);
    ~UploadVideoPage() override;

    void setVideoFile(const QString &videoPath);
    void resetPage();

signals:
    void backToMyPage();

private:
    void initUI();
    void initCategoryData();
    void updateTags(const QString &category);
    void updateTitleCount(const QString &text);
    void updateDescCount();
    void chooseVideo();
    void chooseCover();
    void commitUpload();
    int selectedTagCount() const;

private:
    Ui::UploadVideoPage *ui;
    QString m_videoPath;
    QString m_coverPath;
    QMap<QString, QStringList> m_categoryTags;
};

#endif // UPLOADVIDEOPAGE_H
