// uploadvideopage.h 声明上传视频页面组件。
// UploadVideoPage 负责承载上传视频表单，并把第一版视频元数据提交到临时接口。
#ifndef UPLOADVIDEOPAGE_H
#define UPLOADVIDEOPAGE_H

#include <QString>
#include <QStringList>
#include <QWidget>

namespace Ui {
class UploadVideoPage;
}

class QPushButton;
class QComboBox;
class ApiClient;

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
    void updateTags();
    void addSelectedTag(const QString &tag);
    void updateTitleCount(const QString &text);
    void updateDescCount();
    void chooseVideo();
    void chooseCover();
    void commitUpload();
    void onUploadSucceeded(const QString &message);
    void onUploadFailed(const QString &message);
    int selectedTagCount() const;
    QStringList selectedTags() const;
    void setCommitButtonRequesting(bool requesting);

private:
    Ui::UploadVideoPage *ui;
    QString m_videoPath;
    QString m_coverPath;
    QComboBox *m_tagCombo = nullptr;
    ApiClient *m_apiClient = nullptr;
    bool m_isUploadRequesting = false;
};

#endif // UPLOADVIDEOPAGE_H
