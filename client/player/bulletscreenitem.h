#ifndef BULLETSCREENITEM_H
#define BULLETSCREENITEM_H

#include <QFrame>
#include <QString>

class QLabel;
class QPropertyAnimation;

class BulletScreenItem : public QFrame
{
    Q_OBJECT

public:
    explicit BulletScreenItem(QWidget *parent = nullptr);

    void setBulletScreenText(const QString &text);
    void setBulletScreenAnimation(int startX, int durationMs);
    void startAnimation();

private:
    QLabel *m_textLabel = nullptr;
    QPropertyAnimation *m_animation = nullptr;
};

#endif // BULLETSCREENITEM_H
