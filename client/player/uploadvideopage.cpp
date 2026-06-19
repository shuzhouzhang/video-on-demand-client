// uploadvideopage.cpp 实现上传视频页面流程。
// 当前阶段选择本地文件并把视频元数据提交到临时上传接口，不传真实视频二进制。
#include "uploadvideopage.h"
#include "apiclient.h"
#include "datacenter.h"
#include "ui_uploadvideopage.h"
#include "util.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QLayoutItem>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QStringList>
#include <QTextCursor>

namespace {
constexpr int kMaxTitleLength = 80;
constexpr int kMaxDescLength = 1000;
constexpr int kMaxTagCount = 5;

QString fileNameFromPath(const QString &path)
{
    return QFileInfo(path).fileName();
}
}

UploadVideoPage::UploadVideoPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::UploadVideoPage)
{
    initUI();
}

UploadVideoPage::~UploadVideoPage()
{
    delete ui;
}

void UploadVideoPage::setVideoFile(const QString &videoPath)
{
    m_videoPath = videoPath;

    const QString fileName = fileNameFromPath(videoPath);
    ui->fileNameLabel->setText(fileName);
    ui->uploadProgressLabel->setText("文件已选择");
    ui->downIconLabel->show();
    ui->videoTitleEdit->setText(fileName);

    LOG() << "上传页接收视频文件:" << videoPath;
}

void UploadVideoPage::resetPage()
{
    m_videoPath.clear();
    m_coverPath.clear();

    ui->fileNameLabel->setText("请选择要上传的视频文件");
    ui->uploadProgressLabel->setText("等待选择");
    ui->downIconLabel->hide();
    ui->videoTitleEdit->clear();
    ui->titleCountLabel->setText("0/80");
    ui->descEdit->clear();
    ui->descCountLabel->setText("0/1000");
    ui->coverImageLabel->setPixmap(QPixmap());
    ui->coverImageLabel->setStyleSheet("border-image: url(:/images/uploadVideoPage/videoCoverBg.png);");
    ui->kindCombo->setCurrentIndex(-1);
    updateTags();
}

void UploadVideoPage::initUI()
{
    ui->setupUi(this);
    m_apiClient = new ApiClient(this);

    ui->downIconLabel->setStyleSheet("border-image: url(:/images/uploadVideoPage/wancheng.png);");
    ui->fileIconLabel->setStyleSheet("border-image: url(:/images/uploadVideoPage/wenjian.png);");
    ui->coverIconLabel->setStyleSheet("border-image: url(:/images/uploadVideoPage/fengmian.png);");
    ui->coverImageLabel->setStyleSheet("border-image: url(:/images/uploadVideoPage/videoCoverBg.png);");
    ui->categoryTitleLabel->setText("分类");
    ui->fieldTipLabel->setText("标签可自行选择，最多 5 个");

    ui->kindCombo->addItems(DataCenter::instance().categories());
    ui->kindCombo->setCurrentIndex(-1);

    connect(ui->videoTitleEdit, &QLineEdit::textChanged, this, &UploadVideoPage::updateTitleCount);
    connect(ui->descEdit, &QPlainTextEdit::textChanged, this, &UploadVideoPage::updateDescCount);
    connect(ui->selectVideoBtn, &QPushButton::clicked, this, &UploadVideoPage::chooseVideo);
    connect(ui->changeCoverBtn, &QPushButton::clicked, this, &UploadVideoPage::chooseCover);
    connect(ui->kindCombo, &QComboBox::currentTextChanged, this, &UploadVideoPage::updateTags);
    connect(ui->backBtn, &QPushButton::clicked, this, [this]() {
        resetPage();
        emit backToMyPage();
    });
    connect(ui->commitBtn, &QPushButton::clicked, this, &UploadVideoPage::commitUpload);
    connect(m_apiClient, &ApiClient::uploadSucceeded, this, &UploadVideoPage::onUploadSucceeded);
    connect(m_apiClient, &ApiClient::uploadFailed, this, &UploadVideoPage::onUploadFailed);
    connect(m_apiClient, &ApiClient::uploadProgressChanged, this, [this](int percent) {
        // 这是什么：上传页接收真实文件发送百分比。
        // 为什么能实现：ApiClient 已把字节数换算成 0~100，页面只需更新文字。
        // 什么时候调用：multipart 文件上传过程中由 Qt 信号槽自动调用。
        // 和谁配合：uploadProgressLabel 和 commitBtn 同步告诉用户当前进度。
        ui->uploadProgressLabel->setText(QStringLiteral("上传中 %1%").arg(percent));
        ui->commitBtn->setText(QStringLiteral("上传中 %1%").arg(percent));
    });

    resetPage();

    setStyleSheet(R"(
        QWidget#UploadVideoPage,
        QWidget#uploadRoot,
        QWidget#formArea,
        QWidget#leftForm,
        QWidget#rightForm,
        QWidget#tagWidget {
            background: #ffffff;
        }
        QLabel#pageTitleLabel {
            color: #111827;
            font-size: 24px;
            font-weight: 700;
        }
        QLabel#pageTipLabel,
        QLabel#uploadProgressLabel,
        QLabel#titleCountLabel,
        QLabel#descCountLabel,
        QLabel#fieldTipLabel {
            color: #8b95a1;
            font-size: 13px;
        }
        QLabel#sectionTitleLabel,
        QLabel#coverTitleLabel,
        QLabel#categoryTitleLabel,
        QLabel#descTitleLabel {
            color: #111827;
            font-size: 16px;
            font-weight: 700;
        }
        QLabel#fileNameLabel {
            color: #111827;
            font-size: 15px;
            font-weight: 600;
        }
        QFrame#fileStatusFrame,
        QFrame#coverFrame {
            border: 1px solid #e5edf5;
            border-radius: 8px;
            background: #fbfdff;
        }
        QLineEdit#videoTitleEdit,
        QPlainTextEdit#descEdit,
        QComboBox#kindCombo,
        QComboBox#tagCombo {
            border: 1px solid #dbe7f0;
            border-radius: 6px;
            color: #111827;
            background: #ffffff;
            font-size: 14px;
        }
        QLineEdit#videoTitleEdit {
            min-height: 38px;
            padding-left: 12px;
            padding-right: 12px;
        }
        QPlainTextEdit#descEdit {
            padding: 10px;
        }
        QComboBox#kindCombo,
        QComboBox#tagCombo {
            min-height: 38px;
            padding-left: 12px;
        }
        QLineEdit#videoTitleEdit:focus,
        QPlainTextEdit#descEdit:focus,
        QComboBox#kindCombo:focus,
        QComboBox#tagCombo:focus {
            border-color: #3eceff;
        }
        QPushButton#addTagButton {
            min-width: 62px;
            min-height: 34px;
            border: none;
            border-radius: 17px;
            color: #ffffff;
            background: #3eceff;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton#addTagButton:hover {
            background: #27bee8;
        }
        QPushButton#tagButton {
            min-width: 86px;
            min-height: 34px;
            border: 1px solid #dbe7f0;
            border-radius: 17px;
            color: #374151;
            background: #ffffff;
            font-size: 14px;
        }
        QPushButton#tagButton:checked {
            border-color: #3eceff;
            color: #0ea5d7;
            background: #f3fbff;
        }
        QPushButton#selectVideoBtn,
        QPushButton#changeCoverBtn,
        QPushButton#backBtn {
            border: 1px solid #dbe7f0;
            border-radius: 18px;
            color: #374151;
            background: #ffffff;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton#selectVideoBtn:hover,
        QPushButton#changeCoverBtn:hover,
        QPushButton#backBtn:hover {
            border-color: #3eceff;
            color: #3eceff;
            background: #f3fbff;
        }
        QPushButton#commitBtn {
            border: none;
            border-radius: 18px;
            color: #ffffff;
            background: #3eceff;
            font-size: 14px;
            font-weight: 700;
        }
        QPushButton#commitBtn:hover {
            background: #27bee8;
        }
    )");
}

void UploadVideoPage::updateTags()
{
    m_tagCombo = nullptr;
    while (QLayoutItem *item = ui->tagLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    const QStringList tags = DataCenter::instance().tagsForCategory(ui->kindCombo->currentText());

    m_tagCombo = new QComboBox(ui->tagWidget);
    m_tagCombo->setObjectName("tagCombo");
    m_tagCombo->addItems(tags);
    if (tags.isEmpty()) {
        m_tagCombo->addItem("请先选择分类");
    }
    m_tagCombo->setMinimumWidth(180);
    m_tagCombo->setMaximumWidth(220);
    m_tagCombo->setEnabled(!tags.isEmpty());
    ui->tagLayout->addWidget(m_tagCombo);

    auto *addTagButton = new QPushButton("添加", ui->tagWidget);
    addTagButton->setObjectName("addTagButton");
    addTagButton->setCursor(Qt::PointingHandCursor);
    addTagButton->setEnabled(!tags.isEmpty());
    ui->tagLayout->addWidget(addTagButton);

    connect(addTagButton, &QPushButton::clicked, this, [this]() {
        if (!m_tagCombo) {
            return;
        }

        addSelectedTag(m_tagCombo->currentText());
    });

    ui->tagLayout->addStretch();
}

void UploadVideoPage::addSelectedTag(const QString &tag)
{
    const QString trimmedTag = tag.trimmed();
    if (trimmedTag.isEmpty()) {
        return;
    }

    if (selectedTagCount() >= kMaxTagCount) {
        QMessageBox::information(this, "上传视频", "最多只能选择 5 个标签");
        return;
    }

    const QList<QPushButton *> buttons = ui->tagWidget->findChildren<QPushButton *>("tagButton");
    for (const auto *button : buttons) {
        if (button->text() == trimmedTag) {
            return;
        }
    }

    auto *button = new QPushButton(trimmedTag, ui->tagWidget);
    button->setObjectName("tagButton");
    button->setCheckable(true);
    button->setChecked(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setToolTip("再次点击取消选择");

    const int stretchIndex = qMax(0, ui->tagLayout->count() - 1);
    ui->tagLayout->insertWidget(stretchIndex, button);

    connect(button, &QPushButton::clicked, button, [button]() {
        button->deleteLater();
    });
}

void UploadVideoPage::updateTitleCount(const QString &text)
{
    if (text.length() > kMaxTitleLength) {
        ui->videoTitleEdit->setText(text.left(kMaxTitleLength));
        return;
    }

    ui->titleCountLabel->setText(QString("%1/80").arg(text.length()));
}

void UploadVideoPage::updateDescCount()
{
    const QString text = ui->descEdit->toPlainText();
    if (text.length() > kMaxDescLength) {
        ui->descEdit->setPlainText(text.left(kMaxDescLength));
        QTextCursor cursor = ui->descEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        ui->descEdit->setTextCursor(cursor);
        return;
    }

    ui->descCountLabel->setText(QString("%1/1000").arg(text.length()));
}

void UploadVideoPage::chooseVideo()
{
    const QString fileName = QFileDialog::getOpenFileName(this,
                                                          "上传视频",
                                                          QString(),
                                                          "Videos (*.mp4 *.rmvb *.avi *.mov)");
    if (fileName.isEmpty()) {
        LOG() << "取消选择上传视频文件";
        return;
    }

    const QFileInfo fileInfo(fileName);
    constexpr qint64 maxVideoSize = 4LL * 1024 * 1024 * 1024;
    if (fileInfo.size() > maxVideoSize) {
        QMessageBox::warning(this, "上传视频", "视频大小不能超过 4GB");
        LOG() << "上传视频文件超过 4GB:" << fileName << fileInfo.size();
        return;
    }

    setVideoFile(fileName);
}

void UploadVideoPage::chooseCover()
{
    const QString fileName = QFileDialog::getOpenFileName(this,
                                                          "选择视频封面图",
                                                          QString(),
                                                          "Images (*.png *.jpg *.jpeg)");
    if (fileName.isEmpty()) {
        LOG() << "取消选择视频封面";
        return;
    }

    QPixmap pixmap(fileName);
    if (pixmap.isNull()) {
        QMessageBox::warning(this, "上传视频", "封面图片读取失败");
        return;
    }

    m_coverPath = fileName;
    ui->coverImageLabel->setStyleSheet("border: none;");
    ui->coverImageLabel->setPixmap(pixmap.scaled(ui->coverImageLabel->size(),
                                                 Qt::KeepAspectRatioByExpanding,
                                                 Qt::SmoothTransformation));
    LOG() << "已选择视频封面:" << fileName;
}

void UploadVideoPage::commitUpload()
{
    if (m_isUploadRequesting) {
        return;
    }

    if (m_videoPath.isEmpty()) {
        QMessageBox::warning(this, "上传视频", "请先选择视频文件");
        return;
    }

    const QString title = ui->videoTitleEdit->text().trimmed();
    if (title.isEmpty()) {
        QMessageBox::warning(this, "上传视频", "视频标题不能为空");
        ui->videoTitleEdit->setFocus();
        return;
    }

    const QString category = ui->kindCombo->currentText();
    if (category.isEmpty()) {
        QMessageBox::warning(this, "上传视频", "请选择视频分类");
        ui->kindCombo->setFocus();
        return;
    }

    if (selectedTagCount() > kMaxTagCount) {
        QMessageBox::warning(this, "上传视频", "最多只能选择 5 个标签");
        return;
    }

    if (!DataCenter::instance().isLoggedIn()) {
        QMessageBox::warning(this, "上传视频", "请先登录后再发布");
        return;
    }

    const UserInfo currentUser = DataCenter::instance().currentUser();
    const QFileInfo videoFileInfo(m_videoPath);
    const QFileInfo coverFileInfo(m_coverPath);
    UploadVideoInfo uploadInfo;
    uploadInfo.title = title;
    uploadInfo.description = ui->descEdit->toPlainText().trimmed();
    uploadInfo.category = category;
    uploadInfo.tags = selectedTags();
    uploadInfo.userName = currentUser.userName;
    uploadInfo.account = currentUser.account;
    uploadInfo.videoFileName = videoFileInfo.fileName();
    uploadInfo.coverFileName = coverFileInfo.fileName();
    uploadInfo.videoFilePath = m_videoPath;
    uploadInfo.coverFilePath = m_coverPath;

    // 这是什么：把上传页表单和真实文件路径提交给上传接口。
    // 为什么能实现：ApiClient::uploadVideo() 使用 multipart 同时发送 JSON、视频和可选封面。
    // 什么时候调用：视频、标题、分类、标签数量和登录状态都校验通过后调用。
    // 和谁配合：DataCenter 提供当前用户，ApiClient 负责网络请求，onUploadSucceeded/onUploadFailed 负责收尾。
    setCommitButtonRequesting(true);
    LOG() << "接口发布视频:"
          << "videoPath=" << m_videoPath
          << "title=" << title
          << "category=" << category
          << "coverPath=" << m_coverPath
          << "account=" << currentUser.account;
    m_apiClient->uploadVideo(uploadInfo);
}

void UploadVideoPage::onUploadSucceeded(const QString &message)
{
    // 这是什么：处理真实视频文件上传成功结果。
    // 为什么能实现：ApiClient 已确认 POST /videos/upload 返回 success=true，并把 message 传回页面。
    // 什么时候调用：ApiClient::uploadSucceeded 信号触发时由 Qt 自动调用。
    // 和谁配合：上传按钮恢复、页面重置，并通过 backToMyPage 回到“我的”页面。
    setCommitButtonRequesting(false);
    QMessageBox::information(this, "上传视频", message.isEmpty() ? "发布成功" : message);
    resetPage();
    emit backToMyPage();
}

void UploadVideoPage::onUploadFailed(const QString &message)
{
    // 这是什么：处理真实视频文件上传失败结果。
    // 为什么能实现：ApiClient 会把网络错误、响应格式错误或后端业务失败统一转成 message。
    // 什么时候调用：ApiClient::uploadFailed 信号触发时由 Qt 自动调用。
    // 和谁配合：上传按钮恢复可点，QMessageBox 把失败原因展示给用户。
    setCommitButtonRequesting(false);
    QMessageBox::warning(this, "上传视频", message.isEmpty() ? "发布失败" : message);
}

int UploadVideoPage::selectedTagCount() const
{
    int count = 0;
    const QList<QPushButton *> buttons = ui->tagWidget->findChildren<QPushButton *>();
    for (const auto *button : buttons) {
        if (button->isChecked()) {
            ++count;
        }
    }
    return count;
}

QStringList UploadVideoPage::selectedTags() const
{
    // 这是什么：收集上传页当前已选择的标签文本。
    // 为什么能实现：已选标签都以 objectName=tagButton 的 QPushButton 形式放在 tagWidget 里。
    // 什么时候调用：发布前构造 UploadVideoInfo 时调用。
    // 和谁配合：ApiClient::uploadVideo() 会把返回的 QStringList 转成 JSON 数组发送给后端。
    QStringList tags;
    const QList<QPushButton *> buttons = ui->tagWidget->findChildren<QPushButton *>("tagButton");
    for (const auto *button : buttons) {
        if (button->isChecked()) {
            tags.append(button->text());
        }
    }
    return tags;
}

void UploadVideoPage::setCommitButtonRequesting(bool requesting)
{
    // 这是什么：切换发布按钮的请求中状态。
    // 为什么能实现：网络请求是异步的，禁用按钮可以避免用户连续点击重复上传大文件。
    // 什么时候调用：开始上传前置为 true，上传成功或失败后恢复为 false。
    // 和谁配合：commitUpload()、onUploadSucceeded()、onUploadFailed() 共同维护这一个状态。
    m_isUploadRequesting = requesting;
    ui->commitBtn->setEnabled(!requesting);
    ui->commitBtn->setText(requesting ? "发布中..." : "发布");
}
