#include "player.h"
#include "ui_player.h"

#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QPushButton>
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
    // 先加载 player.ui 里设计好的控件，包括右上角的 minBtn/quitBtn。
    ui->setupUi(this);

    // 让两个窗口按钮只显示自定义图片，不显示 QPushButton 默认边框效果。
    ui->minBtn->setFlat(true);
    ui->quitBtn->setFlat(true);

    // 确保按钮在顶部栏最上层，避免后续页面内容盖住它们。
    ui->minBtn->raise();
    ui->quitBtn->raise();

    // 无边框窗口没有系统标题栏，所以最小化和关闭需要自己连接。
    connect(ui->minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(ui->quitBtn, &QPushButton::clicked, this, &QWidget::close);

    resize(1450, 860);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);

    // contentLayout 来自 player.ui。不能再清空它，否则 .ui 里的顶部栏按钮会被删掉。
    auto *contentLayout = qobject_cast<QVBoxLayout *>(ui->contentWidget->layout());
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    auto *shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setBlurRadius(24);
    shadowEffect->setOffset(0, 8);
    shadowEffect->setColor(QColor(15, 23, 42, 45));
    ui->contentWidget->setGraphicsEffect(shadowEffect);

    setStyleSheet(R"(
        QWidget {
            font-family: "Microsoft YaHei", "Segoe UI", sans-serif;
            color: #111827;
        }
        QFrame#contentWidget {
            background: #ffffff;
            border-radius: 0px;
        }
    )");

    // 这里必须在全局 setStyleSheet() 之后设置。
    // 如果写在 .ui 里，后面的全局样式可能会把按钮图片样式覆盖掉。
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
