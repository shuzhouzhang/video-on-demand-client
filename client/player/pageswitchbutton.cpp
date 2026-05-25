#include "pageswitchbutton.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QStyle>
#include <QVBoxLayout>

PageSwitchButton::PageSwitchButton(QWidget *parent)
    : QWidget{parent}
{
    // 鼠标移到控件上时显示“小手”，提示用户这里可以点击。
    setCursor(Qt::PointingHandCursor);
    setObjectName("pageSwitchButton");
    setProperty("checked", false);
    setProperty("hovered", false);

    // 自定义 QWidget 想用 QSS 画背景色，需要打开这个属性。
    setAttribute(Qt::WA_StyledBackground, true);

    // 左边的 QLabel 用来显示图片。QLabel 既能显示文字，也能显示 QPixmap 图片。
    m_iconLabel = new QLabel(this);
    m_iconLabel->setObjectName("pageSwitchButtonIcon");
    m_iconLabel->setFixedSize(24, 24);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setScaledContents(true);

    // 鼠标事件交给外层 PageSwitchButton 处理，不让 QLabel 抢走点击/悬停事件。
    m_iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    // 右边的 QLabel 用来显示按钮文字，例如“首页”“视频库”。
    m_textLabel = new QLabel(this);
    m_textLabel->setObjectName("pageSwitchButtonText");
    m_textLabel->setAlignment(Qt::AlignCenter);
    m_textLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    // 水平布局：先放图片 QLabel，再放文字 QLabel。
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 6, 0, 6);
    layout->setSpacing(4);
    layout->addWidget(m_iconLabel, 0, Qt::AlignCenter);
    layout->addWidget(m_textLabel, 0, Qt::AlignCenter);

    // checked=true 表示当前页面对应的按钮；hovered=true 表示鼠标正停在按钮上。
    setStyleSheet(R"(
        QWidget#pageSwitchButton {
            background: transparent;
            border-radius: 6px;
        }
        QWidget#pageSwitchButton[hovered="true"] {
            background: #f5f7fb;
        }
        QWidget#pageSwitchButton[checked="true"] {
            background: transparent;
        }
        QLabel#pageSwitchButtonText {
            color: #73777f;
            font-size: 10px;
            font-weight: 700;
            background: transparent;
        }
        QWidget#pageSwitchButton[hovered="true"] QLabel#pageSwitchButtonText {
            color: #111827;
        }
        QWidget#pageSwitchButton[checked="true"] QLabel#pageSwitchButtonText {
            color: #111827;
        }
        QLabel#pageSwitchButtonIcon {
            background: transparent;
        }
    )");
}

PageSwitchButton::PageSwitchButton(const QPixmap &icon, const QString &text, QWidget *parent)
    : PageSwitchButton(parent)
{
    setIcon(icon);
    setText(text);
}

void PageSwitchButton::setIcon(const QPixmap &icon)
{
    if (icon.isNull()) {
        m_iconLabel->clear();
        return;
    }

    // 把传进来的图片缩放到 QLabel 的大小，并保持图片比例不变。
    m_iconLabel->setPixmap(icon.scaled(m_iconLabel->size(),
                                       Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation));
}

void PageSwitchButton::setText(const QString &text)
{
    m_textLabel->setText(text);
}

QString PageSwitchButton::text() const
{
    return m_textLabel->text();
}

void PageSwitchButton::setChecked(bool checked)
{
    if (m_checked == checked) {
        return;
    }

    m_checked = checked;
    setProperty("checked", checked);
    refreshStyle();
}

bool PageSwitchButton::isChecked() const
{
    return m_checked;
}

void PageSwitchButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    setProperty("hovered", true);
    refreshStyle();
    QWidget::enterEvent(event);
}

void PageSwitchButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    setProperty("hovered", false);
    refreshStyle();
    QWidget::leaveEvent(event);
}

void PageSwitchButton::mousePressEvent(QMouseEvent *event)
{
    // 只响应鼠标左键。点击后发出 clicked()，外部就能切换页面。
    if (event->button() == Qt::LeftButton) {
        emit clicked();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void PageSwitchButton::refreshStyle()
{
    // 动态属性 checked/hovered 改变后，Qt 不一定自动重刷 QSS。
    // 外层控件负责背景，两个 QLabel 负责图标/文字，所以都刷新一遍。
    style()->unpolish(this);
    style()->polish(this);
    update();

    m_iconLabel->style()->unpolish(m_iconLabel);
    m_iconLabel->style()->polish(m_iconLabel);
    m_iconLabel->update();

    m_textLabel->style()->unpolish(m_textLabel);
    m_textLabel->style()->polish(m_textLabel);
    m_textLabel->update();
}
