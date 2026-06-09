// player.cpp 实现主窗口逻辑。
// 这里负责初始化 UI、连接按钮事件、切换右侧页面，以及处理无边框窗口拖拽。
#include "player.h"
#include "apiclient.h"
#include "datacenter.h"
#include "login.h"
#include "ui_player.h"
#include "pageswitchbutton.h"
#include "playerpage.h"
#include "uploadvideopage.h"
#include "util.h"
#include "videobox.h"

#include <QAction>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLayoutItem>
#include <QList>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QStringList>
#include <QStyle>
#include <QVBoxLayout>

namespace {
constexpr int kVisibleHomeCategoryCount = 5;

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

void player::clearLayout(QLayout *layout)
{
    if (!layout) {
        return;
    }

    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
}

void player::initHomeFilters()
{
    m_selectedCategory.clear();
    m_selectedTag.clear();
    m_homeVideos = DataCenter::instance().homeVideos();
    refreshHomeCategoryButtons();
    refreshHomeTagButtons();
}

void player::refreshHomeCategoryButtons()
{
    clearLayout(ui->classifyHLayout);
    m_categoryButtons.clear();

    const QStringList categories = DataCenter::instance().categories();

    auto addCategoryButton = [this](const QString &category, bool selected) {
        auto *button = new QPushButton(category, ui->classifys);
        button->setObjectName("homeTextOption");
        button->setCursor(Qt::PointingHandCursor);
        button->setFlat(true);
        button->setProperty("selected", selected);
        button->setMinimumHeight(28);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        button->style()->unpolish(button);
        button->style()->polish(button);
        m_categoryButtons.append(button);
        ui->classifyHLayout->addWidget(button);

        connect(button, &QPushButton::clicked, this, [this, category]() {
            selectHomeCategory(category == "分类" ? QString() : category);
        });

        return button;
    };

    addCategoryButton("分类", m_selectedCategory.isEmpty());
    const int visibleCount = qMin(kVisibleHomeCategoryCount, categories.size());
    for (int i = 0; i < visibleCount; ++i) {
        const QString category = categories.at(i);
        addCategoryButton(category, category == m_selectedCategory);
    }

    if (categories.size() > visibleCount) {
        auto *moreButton = new QPushButton("更多", ui->classifys);
        moreButton->setObjectName("homeTextOption");
        moreButton->setCursor(Qt::PointingHandCursor);
        moreButton->setFlat(true);
        moreButton->setMinimumHeight(28);
        moreButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        auto *moreMenu = new QMenu(moreButton);
        bool moreSelected = false;
        for (int i = visibleCount; i < categories.size(); ++i) {
            const QString category = categories.at(i);
            auto *action = moreMenu->addAction(category);
            connect(action, &QAction::triggered, this, [this, category]() {
                selectHomeCategory(category);
            });
            moreSelected = moreSelected || category == m_selectedCategory;
        }

        moreButton->setProperty("selected", moreSelected);
        moreButton->setMenu(moreMenu);
        moreButton->style()->unpolish(moreButton);
        moreButton->style()->polish(moreButton);
        m_categoryButtons.append(moreButton);
        ui->classifyHLayout->addWidget(moreButton);
    }

    ui->classifyHLayout->addStretch();
    ui->classifys->updateGeometry();
    ui->classifys->update();
}

void player::selectHomeCategory(const QString &category)
{
    m_selectedCategory = category;
    m_selectedTag.clear();
    refreshHomeCategoryButtons();
    refreshHomeTagButtons();
    renderHomeVideos();
    LOG() << "切换分类:" << (m_selectedCategory.isEmpty() ? "全部" : m_selectedCategory);
}

void player::refreshHomeTagButtons()
{
    clearLayout(ui->labelHLayout);
    m_tagButtons.clear();

    if (m_selectedCategory.isEmpty()) {
        ui->labels->hide();
        ui->labels->updateGeometry();
        return;
    }

    ui->labels->show();

    QStringList tags;
    tags.append("标签");
    tags.append(DataCenter::instance().tagsForCategory(m_selectedCategory));

    for (const QString &tag : tags) {
        auto *button = new QPushButton(tag, ui->labels);
        button->setObjectName("homeTextOption");
        button->setCursor(Qt::PointingHandCursor);
        button->setFlat(true);
        button->setProperty("selected", tag == "标签" ? m_selectedTag.isEmpty() : tag == m_selectedTag);
        button->setMinimumHeight(28);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        button->style()->unpolish(button);
        button->style()->polish(button);
        m_tagButtons.append(button);
        ui->labelHLayout->addWidget(button);

        connect(button, &QPushButton::clicked, this, [this, tag]() {
            m_selectedTag = tag == "标签" ? QString() : tag;
            refreshHomeTagButtons();
            renderHomeVideos();
            LOG() << "切换标签:" << (m_selectedTag.isEmpty() ? "全部" : m_selectedTag);
        });
    }

    ui->labelHLayout->addStretch();
    ui->labels->updateGeometry();
    ui->labels->update();
}

void player::renderHomeVideos()
{
    clearLayout(ui->videoScrollLayout);
    ui->videoScrollLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    int visibleIndex = 0;
    for (const VideoInfo &video : m_homeVideos) {
        if (!m_selectedCategory.isEmpty() && video.category != m_selectedCategory) {
            continue;
        }
        if (!m_selectedTag.isEmpty() && !video.tags.contains(m_selectedTag)) {
            continue;
        }

        auto *videoBox = new VideoBox(ui->videoScrollContents);
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

        ui->videoScrollLayout->addWidget(videoBox, visibleIndex / 4, visibleIndex % 4);
        ++visibleIndex;
    }
}

void player::setHomeVideos(const QList<VideoInfo> &videos)
{
    if (videos.isEmpty()) {
        return;
    }

    m_homeVideos = videos;
    renderHomeVideos();
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

    initHomeFilters();
    renderHomeVideos();

    // ApiClient 是首页和后端视频接口的协作者；这里先显示本地数据，再异步尝试替换为接口数据。
    m_apiClient = new ApiClient(this);
    connect(m_apiClient, &ApiClient::videosLoaded, this, &player::setHomeVideos);
    connect(m_apiClient, &ApiClient::requestFailed, this, [this](const QString &message) {
        LOG() << "首页视频接口请求失败，继续使用本地兜底数据:" << message;
    });
    m_apiClient->fetchVideos();

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
    connect(ui->uploadVideoPage, &UploadVideoPage::backToMyPage, this, [switchNavButton]() {
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
        ui->uploadVideoPage->resetPage();
        ui->stackedWidget->setCurrentWidget(ui->uploadPage);
        ui->homePageBtn->setChecked(false);
        ui->homePageBtn->setIcon(QPixmap(":/images/homePage/shouye.png"));
        ui->myPageBtn->setChecked(true);
        ui->myPageBtn->setIcon(QPixmap(":/images/homePage/wodexuan.png"));
        ui->sysPageBtn->setChecked(false);
        ui->sysPageBtn->setIcon(QPixmap(":/images/homePage/admin.png"));
        LOG() << "进入上传视频页面";
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
            min-width: 44px;
            min-height: 26px;
            padding-left: 6px;
            padding-right: 6px;
            border: none;
            color: #555b66;
            background: transparent;
            font-size: 14px;
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
