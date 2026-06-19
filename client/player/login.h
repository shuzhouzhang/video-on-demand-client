// login.h 声明登录窗口组件。
// Login 负责展示账号密码登录界面，并在前端校验通过后发出登录成功信号。
#ifndef LOGIN_H
#define LOGIN_H

#include <QPoint>
#include <QWidget>

class QMouseEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class ApiClient;

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
    // 密码和邮箱验证码都会先做前端格式校验，再通过 ApiClient 请求后端确认登录结果。
    void loginSuccess(const QString &userName, const QString &account);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    enum class Mode {
        Password,
        Email
    };

    void initUI();
    void onLoginButtonClicked();
    void onRegisterButtonClicked();
    void onAuthcodeButtonClicked();
    void onLoginSucceeded(const QString &userName, const QString &account);
    void onLoginFailed(const QString &message);
    void onEmailCodeSent(const QString &authcodeId, const QString &debugCode);
    void onEmailCodeFailed(const QString &message);
    void switchMode(Mode mode);
    void clearInputs();
    void refreshModeButtons();

private:
    Ui::Login *ui;
    QPushButton *m_passwordModeBtn = nullptr;
    QPushButton *m_emailModeBtn = nullptr;
    QLabel *m_emailLabel = nullptr;
    QLineEdit *m_emailEdit = nullptr;
    QPushButton *m_authcodeBtn = nullptr;
    QLabel *m_authcodeLabel = nullptr;
    QLineEdit *m_authcodeEdit = nullptr;
    ApiClient *m_apiClient = nullptr;
    QString m_authcodeId;
    Mode m_mode = Mode::Email;
    bool m_isLoginRequesting = false;
    bool m_isDragging = false;
    QPoint m_dragOffset;
};

#endif // LOGIN_H
