// login.h 声明登录窗口组件。
// Login 负责展示账号密码登录界面，并在前端校验通过后发出登录成功信号。
#ifndef LOGIN_H
#define LOGIN_H

#include <QPoint>
#include <QWidget>

class QMouseEvent;

namespace Ui {
class Login;
}

class Login : public QWidget
{
    Q_OBJECT

public:
    explicit Login(QWidget *parent = nullptr);
    ~Login() override;

    // 每次重新打开登录窗口前清空输入框，避免显示上一次输入的账号密码。
    void reset();

signals:
    // 第一版静态登录不接后端，账号和密码通过前端基础校验后就认为登录成功。
    void loginSuccess(const QString &userName, const QString &account);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void initUI();
    void onLoginButtonClicked();

private:
    Ui::Login *ui;
    bool m_isDragging = false;
    QPoint m_dragOffset;
};

#endif // LOGIN_H
