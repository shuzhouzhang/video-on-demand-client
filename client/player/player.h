// player.h 声明主窗口类。
// 主窗口负责承载顶部栏、左侧导航、右侧页面栈，并处理无边框窗口拖拽。
#ifndef PLAYER_H
#define PLAYER_H

#include <QPoint>
#include <QString>
#include <QWidget>

class Login;
class QEvent;
class QMouseEvent;
class QObject;
class UploadVideoPage;

QT_BEGIN_NAMESPACE
namespace Ui {
// 这个类由 Qt 根据 player.ui 自动生成，里面保存了 ui->xxx 这些控件指针。
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
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // 初始化主界面。
    void initUI();
    void showLoginWindow();
    void updateLoginState(const QString &userName, const QString &account);

private:
    // ui 指向 Qt Designer 生成的界面对象，控件都从这里访问。
    Ui::player *ui;

    // true 表示用户正在按住窗口拖动。
    bool m_isDragging = false;

    // 记录鼠标点下去的位置到窗口左上角的距离。
    // 窗口移动时要靠它计算新位置。
    QPoint m_dragOffset;

    // 第一版登录只保存前端状态，后续接后端时再替换为真实 session/token。
    bool m_isLoggedIn = false;
    QString m_loginUserName;
    QString m_loginAccount;
    Login *m_loginWindow = nullptr;
    UploadVideoPage *m_uploadVideoPage = nullptr;
};

#endif // PLAYER_H
