// uploadvideopage.cpp 实现上传视频页面的静态表单流程。
// 当前阶段只做本地文件选择、表单校验和页面切换，不做真实网络上传。
#include "uploadvideopage.h"
#include "ui_uploadvideopage.h"
#include "util.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QLayoutItem>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
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
    updateTags(QString());
}

void UploadVideoPage::initUI()
{
    ui->setupUi(this);
    initCategoryData();

    ui->downIconLabel->setStyleSheet("border-image: url(:/images/uploadVideoPage/wancheng.png);");
    ui->fileIconLabel->setStyleSheet("border-image: url(:/images/uploadVideoPage/wenjian.png);");
    ui->coverIconLabel->setStyleSheet("border-image: url(:/images/uploadVideoPage/fengmian.png);");
    ui->coverImageLabel->setStyleSheet("border-image: url(:/images/uploadVideoPage/videoCoverBg.png);");

    ui->kindCombo->addItems(m_categoryTags.keys());
    ui->kindCombo->setCurrentIndex(-1);

    connect(ui->videoTitleEdit, &QLineEdit::textChanged, this, &UploadVideoPage::updateTitleCount);
    connect(ui->descEdit, &QPlainTextEdit::textChanged, this, &UploadVideoPage::updateDescCount);
    connect(ui->changeCoverBtn, &QPushButton::clicked, this, &UploadVideoPage::chooseCover);
    connect(ui->kindCombo, &QComboBox::currentTextChanged, this, &UploadVideoPage::updateTags);
    connect(ui->backBtn, &QPushButton::clicked, this, [this]() {
        resetPage();
        emit backToMyPage();
    });
    connect(ui->commitBtn, &QPushButton::clicked, this, &UploadVideoPage::commitUpload);

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
        QComboBox#kindCombo {
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
        QComboBox#kindCombo {
            min-height: 38px;
            padding-left: 12px;
        }
        QLineEdit#videoTitleEdit:focus,
        QPlainTextEdit#descEdit:focus,
        QComboBox#kindCombo:focus {
            border-color: #3eceff;
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
        QPushButton#changeCoverBtn,
        QPushButton#backBtn {
            border: 1px solid #dbe7f0;
            border-radius: 18px;
            color: #374151;
            background: #ffffff;
            font-size: 14px;
            font-weight: 600;
        }
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

void UploadVideoPage::initCategoryData()
{
    m_categoryTags.insert("历史", {"中国史", "世界史", "人物传记", "历史故事", "文化遗产"});
    m_categoryTags.insert("美食", {"美食测评", "美食制作", "地方小吃", "家常菜", "探店"});
    m_categoryTags.insert("游戏", {"游戏攻略", "实况解说", "新手教程", "赛事集锦", "主机游戏"});
    m_categoryTags.insert("科技", {"数码评测", "前沿科技", "编程开发", "人工智能", "软件工具"});
    m_categoryTags.insert("运动", {"健身训练", "篮球", "足球", "跑步", "户外运动"});
    m_categoryTags.insert("动物", {"萌宠日常", "动物世界", "养宠知识", "救助记录", "自然观察"});
    m_categoryTags.insert("旅游", {"北京旅游", "城市漫步", "旅行攻略", "风景记录", "酒店体验"});
    m_categoryTags.insert("电影", {"电影解说", "影评", "预告解析", "幕后故事", "经典片段"});
}

void UploadVideoPage::updateTags(const QString &category)
{
    while (QLayoutItem *item = ui->tagLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    const QStringList tags = m_categoryTags.value(category);
    for (const QString &tag : tags) {
        auto *button = new QPushButton(tag, ui->tagWidget);
        button->setObjectName("tagButton");
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        ui->tagLayout->addWidget(button);

        connect(button, &QPushButton::toggled, this, [this, button](bool checked) {
            if (checked && selectedTagCount() > kMaxTagCount) {
                button->setChecked(false);
                QMessageBox::information(this, "上传视频", "最多只能选择 5 个标签");
            }
        });
    }

    ui->tagLayout->addStretch();
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

    LOG() << "静态发布视频:"
          << "videoPath=" << m_videoPath
          << "title=" << title
          << "category=" << category
          << "coverPath=" << m_coverPath;
    QMessageBox::information(this, "上传视频", "发布成功");

    resetPage();
    emit backToMyPage();
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
