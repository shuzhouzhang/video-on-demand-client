// player.cpp 实现主窗口逻辑。
// 这里负责初始化 UI、连接按钮事件、切换右侧页面，以及处理无边框窗口拖拽。
#include "player.h"
#include "login.h"
#include "ui_player.h"
#include "pageswitchbutton.h"
#include "playerpage.h"
#include "uploadvideopage.h"
#include "util.h"
#include "videobox.h"

#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QList>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QStringList>
#include <QStyle>
#include <QVBoxLayout>

namespace {
QIcon makeCircleAvatarIcon(const QPixmap &source, int size)
{
    if (source.isNull()) {
        return {};
    }

    const QPixmap scaled = source.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const int x = (scaled.width() - size) / 2;
    const int y = (scaled.height() - size) / 2;
    const QPixmap cropped = scaled.copy(x, y, size, size);

    QPixmap circle(size, size);
    circle.fill(Qt::transparent);

    QPainter painter(&circle);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addEllipse(0, 0, size, size);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, cropped);

    return QIcon(circle);
}
}

player::player(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::player)
{
    initUI();
}

player::~player()
{
    delete ui;
}

void player::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !isFullScreen()) {
        m_isDragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void player::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging && !isFullScreen()) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void player::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

bool player::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->myNickNameLabel && event->type() == QEvent::MouseButtonRelease) {
        if (!m_isLoggedIn) {
            showLoginWindow();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void player::showLoginWindow()
{
    if (m_isLoggedIn) {
        return;
    }

    if (m_loginWindow == nullptr) {
        m_loginWindow = new Login(this);
        m_loginWindow->setAttribute(Qt::WA_DeleteOnClose);

        connect(m_loginWindow, &Login::loginSuccess, this, &player::updateLoginState);
        connect(m_loginWindow, &QObject::destroyed, this, [this]() {
            m_loginWindow = nullptr;
        });
    }

    m_loginWindow->reset();
    m_loginWindow->show();
    m_loginWindow->raise();
    m_loginWindow->activateWindow();
}

void player::updateLoginState(const QString &userName, const QString &account)
{
    m_isLoggedIn = true;
    m_loginUserName = userName;
    m_loginAccount = account;

    ui->myNickNameLabel->setText(userName);
    ui->myAccountLabel->setText("账号：" + account);
    ui->myDescLabel->setText("欢迎回来，开始管理你的个人资料和视频内容吧");

    LOG() << "我的页面已切换到登录状态:" << account;
}

void player::initUI()
{
    // setupUi() 会读取 player.ui 生成的界面结构，并把控件挂到 ui 指针上。
    ui->setupUi(this);

    // 顶部栏左侧 logo 和标题图片来自 imageRes.qrc 里的 Qt 资源路径。
    ui->logo->setStyleSheet("border-image: url(:/images/homePage/logo.png);");
    ui->titleLogo->setStyleSheet("border-image: url(:/images/homePage/biteshipin.png);");

    // 初始化左侧导航按钮的文字和默认图标。
    ui->homePageBtn->setText("首页");
    ui->homePageBtn->setIcon(QPixmap(":/images/homePage/shouyexuan.png"));
    ui->homePageBtn->setChecked(true);

    ui->myPageBtn->setText("我的");
    ui->myPageBtn->setIcon(QPixmap(":/images/homePage/wode.png"));

    ui->sysPageBtn->setText("后台");
    ui->sysPageBtn->setIcon(QPixmap(":/images/homePage/admin.png"));

    // 我的页面第一版只展示静态个人中心，先用本地资源和假数据把结构搭起来。
    auto setMyAvatar = [this](const QPixmap &pixmap) {
        ui->myAvatarBtn->setStyleSheet(R"(
            QPushButton#myAvatarBtn {
                border: none;
                border-radius: 48px;
                background: #ffffff;
            }
            QPushButton#myAvatarBtn:hover {
                background: #f3fbff;
            }
        )");
        ui->myAvatarBtn->setIcon(makeCircleAvatarIcon(pixmap, 96));
        ui->myAvatarBtn->setIconSize(QSize(96, 96));
    };
    setMyAvatar(QPixmap(":/images/myself/defaultAvatar.png"));
    ui->myNickNameLabel->setText("点击登录");
    ui->myAccountLabel->setText("游客模式");
    ui->myDescLabel->setText("登录后可以修改资料、上传视频和查看个人内容");
    ui->myNickNameLabel->setCursor(Qt::PointingHandCursor);
    ui->myNickNameLabel->installEventFilter(this);

    ui->editProfileBtn->setIcon(QIcon(":/images/myself/bianji.png"));
    ui->editProfileBtn->setIconSize(QSize(16, 16));

    ui->uploadEntryBtn->setIcon(QIcon(":/images/myself/shangchuanshipin.png"));
    ui->uploadEntryBtn->setIconSize(QSize(28, 28));
    ui->myVideoEntryBtn->setIcon(QIcon(":/images/myself/gengduo.png"));
    ui->myVideoEntryBtn->setIconSize(QSize(28, 28));
    ui->followEntryBtn->setIcon(QIcon(":/images/myself/guanzhu.png"));
    ui->followEntryBtn->setIconSize(QSize(28, 28));
    ui->settingEntryBtn->setIcon(QIcon(":/images/myself/shezhi.png"));
    ui->settingEntryBtn->setIconSize(QSize(28, 28));

    m_uploadVideoPage = new UploadVideoPage(ui->stackedWidget);
    ui->stackedWidget->addWidget(m_uploadVideoPage);

    auto refreshButtonStyle = [](QPushButton *button) {
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    };

    auto setupTextButtonGroup = [refreshButtonStyle](QHBoxLayout *layout,
                                                     const QStringList &texts,
                                                     const QString &logPrefix) {
        QList<QPushButton *> buttons;

        for (int i = 0; i < texts.size(); ++i) {
            auto *button = new QPushButton(texts[i]);
            button->setObjectName("homeTextOption");
            button->setCursor(Qt::PointingHandCursor);
            button->setFlat(true);
            button->setProperty("selected", i == 0);
            button->setMinimumHeight(30);
            button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            buttons.append(button);
            layout->addWidget(button);
        }

        for (auto *button : buttons) {
            QObject::connect(button, &QPushButton::clicked, button, [buttons, button, refreshButtonStyle, logPrefix]() {
                for (auto *item : buttons) {
                    item->setProperty("selected", item == button);
                    refreshButtonStyle(item);
                }

                LOG() << logPrefix << button->text();
            });
        }

        layout->addStretch();
    };

    setupTextButtonGroup(ui->classifyHLayout,
                         {"分类", "历史", "美食", "游戏", "科技", "运动", "动物", "旅游", "电影"},
                         "切换分类:");
    setupTextButtonGroup(ui->labelHLayout,
                         {"标签", "中国史", "世界史", "美食测评", "美食制作", "游戏攻略"},
                         "切换标签:");

    ui->videoScrollLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    struct StaticVideoInfo {
        QString title;
        QString userName;
        QString date;
        QString duration;
        QString playCount;
        QString likeCount;
    };

    const QList<StaticVideoInfo> videos = {
        {"【北京旅游攻略】一条视频告诉你去了北京该怎么玩~", "用户昵称", "9-16", "25:52", "26.1万", "1226"},
        {"一条视频告诉你去了北京该怎么玩~", "用户昵称", "9-16", "25:52", "26.1万", "1226"},
        {"世界史入门：从文明起源讲到现代", "用户昵称", "9-16", "18:36", "18.8万", "935"},
        {"美食测评：北京胡同里的宝藏小店", "用户昵称", "9-16", "12:08", "9.7万", "521"},
        {"游戏攻略：新手也能快速上手的通关路线", "用户昵称", "9-16", "21:47", "32.4万", "2048"},
        {"科技观察：一分钟看懂智能设备新趋势", "用户昵称", "9-16", "08:45", "7.2万", "318"},
        {"运动训练：每天十分钟改善体态", "用户昵称", "9-16", "16:20", "11.3万", "746"},
        {"动物世界：森林里的奇妙一天", "用户昵称", "9-16", "14:33", "15.6万", "889"},
    };

    for (int i = 0; i < videos.size(); ++i) {
        auto *videoBox = new VideoBox(ui->videoScrollContents);
        const auto &video = videos[i];
        videoBox->setVideoInfo(video.title,
                               video.userName,
                               video.date,
                               video.duration,
                               video.playCount,
                               video.likeCount);

        connect(videoBox,
                &VideoBox::videoClicked,
                this,
                [](const QString &title,
                   const QString &userName,
                   const QString &date,
                   const QString &duration,
                   const QString &playCount,
                   const QString &likeCount) {
                    auto *playerPage = new PlayerPage(title, userName, date, duration, playCount, likeCount);
                    playerPage->setAttribute(Qt::WA_DeleteOnClose);
                    playerPage->show();
                });

        ui->videoScrollLayout->addWidget(videoBox, i / 4, i % 4);
    }

    auto switchNavButton = [this](int index) {
        // 左侧导航按钮和右侧页面栈保持同一个 index，后续扩展新页面也更直观。
        ui->stackedWidget->setCurrentIndex(index);
        const QStringList pageNames = {"Home", "My", "Admin", "Upload"};
        LOG() << "切换页面:" << pageNames.value(index, "未知页面");

        // 首页按钮：index 为 0 时使用选中图标，否则使用普通图标。
        ui->homePageBtn->setChecked(index == 0);
        ui->homePageBtn->setIcon(QPixmap(index == 0
                                             ? ":/images/homePage/shouyexuan.png"
                                             : ":/images/homePage/shouye.png"));

        // 我的按钮：index 为 1 时使用选中图标，否则使用普通图标。
        ui->myPageBtn->setChecked(index == 1 || index == 3);
        ui->myPageBtn->setIcon(QPixmap((index == 1 || index == 3)
                                           ? ":/images/homePage/wodexuan.png"
                                           : ":/images/homePage/wode.png"));

        // 后台按钮：index 为 2 时使用选中图标，否则使用普通图标。
        ui->sysPageBtn->setChecked(index == 2);
        ui->sysPageBtn->setIcon(QPixmap(index == 2
                                            ? ":/images/homePage/adminxuan.png"
                                            : ":/images/homePage/admin.png"));
    };

    // 默认进入首页。
    switchNavButton(0);

    // 三个导航按钮只负责告诉页面栈要切到哪个页面。
    connect(ui->homePageBtn, &PageSwitchButton::clicked, this, [switchNavButton]() {
        switchNavButton(0);
    });
    connect(ui->myPageBtn, &PageSwitchButton::clicked, this, [switchNavButton]() {
        switchNavButton(1);
    });
    connect(ui->sysPageBtn, &PageSwitchButton::clicked, this, [switchNavButton]() {
        switchNavButton(2);
    });
    connect(m_uploadVideoPage, &UploadVideoPage::backToMyPage, this, [switchNavButton]() {
        switchNavButton(1);
    });

    // 无边框窗口没有系统标题栏，右上角窗口按钮需要自己接系统行为。
    ui->minBtn->setFlat(true);
    ui->quitBtn->setFlat(true);
    ui->minBtn->raise();
    ui->quitBtn->raise();
    connect(ui->minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(ui->quitBtn, &QPushButton::clicked, this, &QWidget::close);
    connect(ui->searchBtn, &QPushButton::clicked, this, [this]() {
        LOG() << "点击搜索按钮，关键词:" << ui->searchEdit->text();
    });
    connect(ui->myAvatarBtn, &QPushButton::clicked, this, [this, setMyAvatar]() {
        if (!m_isLoggedIn) {
            showLoginWindow();
            return;
        }

        const QString fileName = QFileDialog::getOpenFileName(this,
                                                              "修改头像",
                                                              QString(),
                                                              "Images (*.png *.jpg *.jpeg)");
        if (fileName.isEmpty()) {
            LOG() << "取消选择头像文件";
            return;
        }

        const QFileInfo fileInfo(fileName);
        if (fileInfo.size() >= 1024 * 1024 * 5) {
            QMessageBox::warning(this, "修改头像", "头像大小不能超过 5MB");
            LOG() << "头像大小超过限制:" << fileName << fileInfo.size();
            return;
        }

        const QPixmap avatar(fileName);
        if (avatar.isNull()) {
            QMessageBox::warning(this, "修改头像", "头像图片读取失败");
            LOG() << "头像图片读取失败:" << fileName;
            return;
        }

        setMyAvatar(avatar);
        LOG() << "本地头像预览已更新:" << fileName;
    });
    connect(ui->editProfileBtn, &QPushButton::clicked, this, [this]() {
        if (!m_isLoggedIn) {
            showLoginWindow();
            return;
        }

        LOG() << "点击编辑资料按钮，当前阶段暂不打开编辑资料页";
    });
    connect(ui->uploadEntryBtn, &QPushButton::clicked, this, [this]() {
        if (!m_isLoggedIn) {
            showLoginWindow();
            return;
        }

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

        m_uploadVideoPage->resetPage();
        m_uploadVideoPage->setVideoFile(fileName);
        ui->stackedWidget->setCurrentWidget(m_uploadVideoPage);
        ui->homePageBtn->setChecked(false);
        ui->homePageBtn->setIcon(QPixmap(":/images/homePage/shouye.png"));
        ui->myPageBtn->setChecked(true);
        ui->myPageBtn->setIcon(QPixmap(":/images/homePage/wodexuan.png"));
        ui->sysPageBtn->setChecked(false);
        ui->sysPageBtn->setIcon(QPixmap(":/images/homePage/admin.png"));
        LOG() << "进入上传视频页面:" << fileName;
        return;

        LOG() << "点击上传视频入口，当前阶段暂不打开上传页";
    });
    connect(ui->myVideoEntryBtn, &QPushButton::clicked, this, [this]() {
        if (!m_isLoggedIn) {
            showLoginWindow();
            return;
        }

        LOG() << "点击我的视频入口，当前阶段暂不加载作品列表";
    });
    connect(ui->followEntryBtn, &QPushButton::clicked, this, [this]() {
        if (!m_isLoggedIn) {
            showLoginWindow();
            return;
        }

        LOG() << "点击我的关注入口，当前阶段暂不加载关注列表";
    });
    connect(ui->settingEntryBtn, &QPushButton::clicked, this, [this]() {
        if (!m_isLoggedIn) {
            showLoginWindow();
            return;
        }

        LOG() << "点击设置入口，当前阶段暂不打开设置页";
    });

    resize(1450, 860);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);

    // contentLayout 来自 player.ui。不能清空它，否则 UI 里的顶部栏和左侧导航会被删掉。
    auto *contentLayout = qobject_cast<QVBoxLayout *>(ui->contentWidget->layout());
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    auto *shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setBlurRadius(24);
    shadowEffect->setOffset(0, 8);
    shadowEffect->setColor(QColor(15, 23, 42, 45));
    ui->contentWidget->setGraphicsEffect(shadowEffect);

    // 主窗口的基础样式。这里统一设置白色背景、搜索框和占位页文字样式。
    setStyleSheet(R"(
        QWidget {
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
            color: #111827;
        }
        QFrame#contentWidget {
            background: #ffffff;
            border-radius: 0px;
        }
        QWidget#head,
        QWidget#bodyLeft,
        QWidget#bodyRight,
        QStackedWidget#stackedWidget,
        QWidget#homePage,
        QWidget#myPage,
        QWidget#adminPage,
        QWidget#selectSearchBox,
        QWidget#selectBox,
        QWidget#classifys,
        QWidget#labels,
        QWidget#searchBox,
        QWidget#videoBox,
        QScrollArea#videoScroll,
        QWidget#videoScrollContents,
        QWidget#myProfileBox,
        QWidget#myInfoBox,
        QWidget#myStatsBox,
        QWidget#myWorksStatBox,
        QWidget#myFollowStatBox,
        QWidget#myLikeStatBox,
        QWidget#myActionBox,
        QWidget#myWorksBox {
            background: #ffffff;
        }
        QLineEdit#searchEdit {
            min-height: 32px;
            padding-left: 14px;
            padding-right: 12px;
            border: 1px solid #dbe7f0;
            border-top-left-radius: 16px;
            border-bottom-left-radius: 16px;
            border-top-right-radius: 0px;
            border-bottom-right-radius: 0px;
            color: #333333;
            background: #ffffff;
            font-size: 14px;
        }
        QLineEdit#searchEdit:focus {
            border: 1px solid #3eceff;
        }
        QScrollArea#videoScroll {
            border: none;
        }
        QPushButton#homeTextOption {
            min-width: 52px;
            padding-left: 8px;
            padding-right: 8px;
            border: none;
            color: #555b66;
            background: transparent;
            font-size: 15px;
            font-weight: 600;
        }
        QPushButton#homeTextOption[selected="true"] {
            color: #3eceff;
        }
        QPushButton#homeTextOption:hover {
            color: #111827;
        }
        QPushButton#searchBtn {
            border: none;
            border-top-right-radius: 16px;
            border-bottom-right-radius: 16px;
            color: #ffffff;
            background: #3eceff;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton#searchBtn:hover {
            background: #26bce9;
        }
        QLabel#myPageLabel,
        QLabel#adminPageLabel {
            color: #8b95a1;
            font-size: 22px;
            font-weight: 600;
            background: transparent;
        }
        QLabel#myNickNameLabel {
            color: #111827;
            font-size: 24px;
            font-weight: 700;
            background: transparent;
        }
        QLabel#myAccountLabel,
        QLabel#myDescLabel,
        QLabel#myWorksTextLabel,
        QLabel#myFollowTextLabel,
        QLabel#myLikeTextLabel,
        QLabel#myWorksEmptyLabel {
            color: #8b95a1;
            font-size: 14px;
            background: transparent;
        }
        QLabel#myWorksCountLabel,
        QLabel#myFollowCountLabel,
        QLabel#myLikeCountLabel {
            color: #111827;
            font-size: 22px;
            font-weight: 700;
            background: transparent;
        }
        QLabel#myWorksTitleLabel {
            color: #111827;
            font-size: 18px;
            font-weight: 700;
            background: transparent;
        }
        QLabel#myWorksEmptyLabel {
            border: 1px dashed #dbe7f0;
            border-radius: 8px;
            background: #fbfdff;
        }
        QPushButton#editProfileBtn {
            border: 1px solid #dbe7f0;
            border-radius: 17px;
            color: #374151;
            background: #ffffff;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton#editProfileBtn:hover {
            border-color: #3eceff;
            color: #3eceff;
            background: #f3fbff;
        }
        QPushButton#uploadEntryBtn,
        QPushButton#myVideoEntryBtn,
        QPushButton#followEntryBtn,
        QPushButton#settingEntryBtn {
            border: 1px solid #eef2f7;
            border-radius: 8px;
            color: #111827;
            background: #ffffff;
            font-size: 15px;
            font-weight: 600;
            padding-left: 18px;
            padding-right: 18px;
        }
        QPushButton#uploadEntryBtn:hover,
        QPushButton#myVideoEntryBtn:hover,
        QPushButton#followEntryBtn:hover,
        QPushButton#settingEntryBtn:hover {
            border-color: #3eceff;
            background: #f3fbff;
        }
    )");

    // 参考原客户端：在首页右下方放一个悬浮工具条，上面置顶，下面刷新。
    auto *refreshTopWidget = new QWidget(ui->homePage);
    refreshTopWidget->setFixedSize(42, 94);
    refreshTopWidget->move(1278, 618);

    auto *refreshTopLayout = new QVBoxLayout(refreshTopWidget);
    refreshTopLayout->setContentsMargins(0, 0, 0, 0);
    refreshTopLayout->setSpacing(10);

    auto *topBtn = new QPushButton(refreshTopWidget);
    topBtn->setFixedSize(42, 42);
    topBtn->setCursor(Qt::PointingHandCursor);
    topBtn->setIcon(QIcon(":/images/homePage/zhiding.png"));
    topBtn->setIconSize(QSize(42, 42));
    topBtn->setStyleSheet(R"(
        QPushButton {
            border: 1px solid #eef2f7;
            border-radius: 21px;
            background-color: rgba(221, 221, 221, 0.65);
        }
        QPushButton:hover {
            background-color: rgba(102, 102, 102, 0.35);
        }
    )");
    refreshTopLayout->addWidget(topBtn);

    auto *refreshBtn = new QPushButton(refreshTopWidget);
    refreshBtn->setFixedSize(42, 42);
    refreshBtn->setCursor(Qt::PointingHandCursor);
    refreshBtn->setIcon(QIcon(":/images/homePage/shuaxin.png"));
    refreshBtn->setIconSize(QSize(42, 42));
    refreshBtn->setStyleSheet(topBtn->styleSheet());
    refreshTopLayout->addWidget(refreshBtn);
    refreshTopWidget->raise();

    connect(topBtn, &QPushButton::clicked, this, [this]() {
        ui->videoScroll->verticalScrollBar()->setValue(0);
        LOG() << "点击置顶按钮，视频列表回到顶部";
    });
    connect(refreshBtn, &QPushButton::clicked, this, [this]() {
        ui->videoScroll->verticalScrollBar()->setValue(0);
        LOG() << "点击刷新按钮，当前阶段仅回到顶部，暂不重新请求视频列表";
    });

    // 放在全局样式后面设置，避免窗口全局样式覆盖按钮图片。
    ui->minBtn->setStyleSheet(R"(
        QPushButton#minBtn {
            border: none;
            border-image: url(:/images/homePage/suoxiao.png);
        }
    )");
    ui->quitBtn->setStyleSheet(R"(
        QPushButton#quitBtn {
            border: none;
            border-image: url(:/images/homePage/quxiao.png);
        }
    )");
}
