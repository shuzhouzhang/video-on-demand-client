// player.cpp 实现主窗口逻辑。
// 这里负责初始化 UI、连接按钮事件、切换右侧页面，以及处理无边框窗口拖拽。
#include "player.h"
#include "ui_player.h"
#include "pageswitchbutton.h"
#include "util.h"

#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

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

    auto switchNavButton = [this](int index) {
        // 左侧导航按钮和右侧页面栈保持同一个 index，后续扩展新页面也更直观。
        ui->stackedWidget->setCurrentIndex(index);
        const QStringList pageNames = {"首页", "我的", "后台"};
        LOG() << "切换页面:" << pageNames.value(index, "未知页面");

        // 首页按钮：index 为 0 时使用选中图标，否则使用普通图标。
        ui->homePageBtn->setChecked(index == 0);
        ui->homePageBtn->setIcon(QPixmap(index == 0
                                             ? ":/images/homePage/shouyexuan.png"
                                             : ":/images/homePage/shouye.png"));

        // 我的按钮：index 为 1 时使用选中图标，否则使用普通图标。
        ui->myPageBtn->setChecked(index == 1);
        ui->myPageBtn->setIcon(QPixmap(index == 1
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

    // 无边框窗口没有系统标题栏，右上角窗口按钮需要自己接系统行为。
    ui->minBtn->setFlat(true);
    ui->quitBtn->setFlat(true);
    ui->minBtn->raise();
    ui->quitBtn->raise();
    connect(ui->minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(ui->quitBtn, &QPushButton::clicked, this, &QWidget::close);

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
        QWidget#videoScrollContents {
            background: #ffffff;
        }
        QLineEdit#searchEdit {
            min-height: 32px;
            padding-left: 14px;
            padding-right: 14px;
            border: 1px solid #dbe7f0;
            border-radius: 16px;
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
        QLabel#myPageLabel,
        QLabel#adminPageLabel {
            color: #8b95a1;
            font-size: 22px;
            font-weight: 600;
            background: transparent;
        }
    )");

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
