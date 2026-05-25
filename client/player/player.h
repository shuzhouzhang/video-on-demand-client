#ifndef PLAYER_H
#define PLAYER_H

#include <QPoint>
#include <QWidget>

class QMouseEvent;

QT_BEGIN_NAMESPACE
namespace Ui {
class player;
}
QT_END_NAMESPACE

class player : public QWidget
{
    Q_OBJECT

public:
    explicit player(QWidget *parent = nullptr);
    ~player() override;

protected:
    // 鼠标按下、移动、松开这三个事件，用来实现无边框窗口拖拽。
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    // 初始化主界面。
    void initUI();

private:
    Ui::player *ui;

    // true 表示用户正在按住窗口拖动。
    bool m_isDragging = false;

    // 记录鼠标点下去的位置到窗口左上角的距离。
    // 窗口移动时要靠它计算新位置。
    QPoint m_dragOffset;
};

#endif // PLAYER_H
