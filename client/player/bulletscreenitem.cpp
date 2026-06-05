#include "bulletscreenitem.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPoint>
#include <QPropertyAnimation>

BulletScreenItem::BulletScreenItem(QWidget *parent)
    : QFrame(parent)
{
    setObjectName("bulletScreenItem");
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedHeight(34);
    setStyleSheet(R"(
        QFrame#bulletScreenItem {
            border: none;
            background: transparent;
        }
        QLabel#bulletScreenText {
            color: #ffffff;
            font-size: 18px;
            font-weight: 500;
            background: transparent;
        }
    )");

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(0);

    m_textLabel = new QLabel(this);
    m_textLabel->setObjectName("bulletScreenText");
    m_textLabel->setAttribute(Qt::WA_TranslucentBackground);
    layout->addWidget(m_textLabel);
}

void BulletScreenItem::setBulletScreenText(const QString &text)
{
    m_textLabel->setText(text);
    m_textLabel->adjustSize();
    adjustSize();
    setFixedWidth(sizeHint().width());
}

void BulletScreenItem::setBulletScreenAnimation(int startX, int durationMs)
{
    if (m_animation) {
        m_animation->stop();
        m_animation->deleteLater();
    }

    move(startX, 0);
    m_animation = new QPropertyAnimation(this, "pos", this);
    m_animation->setDuration(durationMs);
    m_animation->setStartValue(QPoint(startX, 0));
    m_animation->setEndValue(QPoint(-width(), 0));
    connect(m_animation, &QPropertyAnimation::finished, this, &BulletScreenItem::deleteLater);
}

void BulletScreenItem::startAnimation()
{
    show();
    raise();
    if (m_animation) {
        m_animation->start();
    }
}
