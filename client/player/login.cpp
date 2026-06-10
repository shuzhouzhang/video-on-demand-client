// login.cpp 实现登录窗口流程。
// 当前阶段密码登录接临时 /login 接口，邮箱验证码登录仍保留本地模拟。
#include "login.h"
#include "apiclient.h"
#include "ui_login.h"
#include "util.h"

#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QStyle>
#include <QVBoxLayout>

namespace {
struct AccountInfo {
    QString nickname;
    QString password;
};

QHash<QString, AccountInfo> &accountStore()
{
    static QHash<QString, AccountInfo> users = {
        {"bit-user-001", {"BIT 用户", "bit123456"}},
    };
    return users;
}

QString accountRuleError(const QString &account)
{
    if (account.isEmpty()) {
        return "账号不能为空";
    }

    if (account.length() < 3 || account.length() > 32) {
        return "账号长度需要在 3 到 32 位之间";
    }

    return {};
}

bool isEmailValid(const QString &email)
{
    static const QRegularExpression pattern(R"(^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$)");
    return pattern.match(email).hasMatch();
}

QString passwordRuleError(const QString &password)
{
    if (password.isEmpty()) {
        return "密码不能为空";
    }

    if (password.length() < 6 || password.length() > 20) {
        return "密码长度需要在 6 到 20 位之间";
    }

    bool hasLetter = false;
    bool hasDigit = false;

    for (const QChar &ch : password) {
        if (ch.isLetter()) {
            hasLetter = true;
        } else if (ch.isDigit()) {
            hasDigit = true;
        }
    }

    if (!hasLetter || !hasDigit) {
        return "密码需要同时包含字母和数字";
    }

    return {};
}

QString nicknameFromEmail(const QString &email)
{
    const int atIndex = email.indexOf('@');
    if (atIndex <= 0) {
        return email;
    }

    return email.left(atIndex);
}

QString makeAuthcode()
{
    const int value = QRandomGenerator::global()->bounded(0, 1000000);
    return QString("%1").arg(value, 6, 10, QLatin1Char('0'));
}
}

Login::Login(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Login)
{
    initUI();
}

Login::~Login()
{
    delete ui;
}

void Login::reset()
{
    switchMode(Mode::Email);
    clearInputs();
    m_emailEdit->setFocus();
}

void Login::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !isFullScreen()) {
        m_isDragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void Login::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging && !isFullScreen()) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void Login::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void Login::initUI()
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setFixedSize(430, 520);

    ui->logoLabel->setStyleSheet("border-image: url(:/images/login/biteshipin.png);");
    ui->accountEdit->setPlaceholderText("请输入邮箱或用户昵称");
    ui->passwordEdit->setEchoMode(QLineEdit::Password);
    m_apiClient = new ApiClient(this);

    auto *modeWidget = new QWidget(this);
    auto *modeLayout = new QHBoxLayout(modeWidget);
    modeLayout->setContentsMargins(0, 0, 0, 0);
    modeLayout->setSpacing(0);

    m_passwordModeBtn = new QPushButton("密码登录", modeWidget);
    m_passwordModeBtn->setObjectName("loginModeButton");
    m_passwordModeBtn->setCursor(Qt::PointingHandCursor);
    m_passwordModeBtn->setMinimumHeight(38);

    m_emailModeBtn = new QPushButton("邮箱登录", modeWidget);
    m_emailModeBtn->setObjectName("loginModeButton");
    m_emailModeBtn->setCursor(Qt::PointingHandCursor);
    m_emailModeBtn->setMinimumHeight(38);

    modeLayout->addWidget(m_passwordModeBtn);
    modeLayout->addWidget(m_emailModeBtn);

    m_emailLabel = new QLabel("邮箱", this);
    m_emailLabel->setObjectName("emailLabel");
    m_emailEdit = new QLineEdit(this);
    m_emailEdit->setObjectName("emailEdit");
    m_emailEdit->setPlaceholderText("请输入邮箱");

    m_authcodeBtn = new QPushButton("获取验证码", this);
    m_authcodeBtn->setObjectName("authcodeBtn");
    m_authcodeBtn->setCursor(Qt::PointingHandCursor);
    m_authcodeBtn->setMinimumHeight(34);

    m_authcodeLabel = new QLabel("验证码", this);
    m_authcodeLabel->setObjectName("authcodeLabel");
    m_authcodeEdit = new QLineEdit(this);
    m_authcodeEdit->setObjectName("authcodeEdit");
    m_authcodeEdit->setPlaceholderText("请输入验证码");
    m_authcodeEdit->setMaxLength(6);

    const int accountLabelIndex = ui->cardLayout->indexOf(ui->accountLabel);
    ui->cardLayout->insertWidget(accountLabelIndex, modeWidget);
    ui->cardLayout->insertWidget(accountLabelIndex + 1, m_emailLabel);
    ui->cardLayout->insertWidget(accountLabelIndex + 2, m_emailEdit);
    ui->cardLayout->insertWidget(accountLabelIndex + 3, m_authcodeBtn);
    ui->cardLayout->insertWidget(accountLabelIndex + 4, m_authcodeLabel);
    ui->cardLayout->insertWidget(accountLabelIndex + 5, m_authcodeEdit);

    ui->accountBottomSpacer->changeSize(20, 18, QSizePolicy::Minimum, QSizePolicy::Fixed);
    ui->passwordBottomSpacer->changeSize(20, 24, QSizePolicy::Minimum, QSizePolicy::Fixed);

    ui->minBtn->setFlat(true);
    ui->quitBtn->setFlat(true);

    connect(ui->minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(ui->quitBtn, &QPushButton::clicked, this, &QWidget::close);
    connect(ui->loginBtn, &QPushButton::clicked, this, &Login::onLoginButtonClicked);
    connect(ui->registerBtn, &QPushButton::clicked, this, &Login::onRegisterButtonClicked);
    connect(m_authcodeBtn, &QPushButton::clicked, this, &Login::onAuthcodeButtonClicked);
    connect(m_passwordModeBtn, &QPushButton::clicked, this, [this]() {
        switchMode(Mode::Password);
    });
    connect(m_emailModeBtn, &QPushButton::clicked, this, [this]() {
        switchMode(Mode::Email);
    });
    connect(ui->accountEdit, &QLineEdit::returnPressed, this, &Login::onLoginButtonClicked);
    connect(ui->passwordEdit, &QLineEdit::returnPressed, this, &Login::onLoginButtonClicked);
    connect(m_emailEdit, &QLineEdit::returnPressed, this, &Login::onLoginButtonClicked);
    connect(m_authcodeEdit, &QLineEdit::returnPressed, this, &Login::onLoginButtonClicked);
    connect(m_apiClient, &ApiClient::loginSucceeded, this, &Login::onLoginSucceeded);
    connect(m_apiClient, &ApiClient::loginFailed, this, &Login::onLoginFailed);

    setStyleSheet(R"(
        QWidget#Login {
            background: #f5fbff;
        }
        QFrame#loginCard {
            background: #ffffff;
            border: 1px solid #e5edf5;
            border-radius: 12px;
        }
        QLabel#titleLabel {
            color: #111827;
            font-size: 24px;
            font-weight: 700;
        }
        QLabel#subTitleLabel,
        QLabel#registerHintLabel {
            color: #8b95a1;
            font-size: 13px;
        }
        QLabel#accountLabel,
        QLabel#passwordLabel,
        QLabel#emailLabel,
        QLabel#authcodeLabel {
            color: #374151;
            font-size: 14px;
            font-weight: 600;
        }
        QLineEdit {
            min-height: 40px;
            padding-left: 12px;
            padding-right: 12px;
            border: 1px solid #dbe7f0;
            border-radius: 6px;
            color: #111827;
            background: #ffffff;
            font-size: 14px;
        }
        QLineEdit:focus {
            border-color: #3eceff;
        }
        QPushButton#loginModeButton {
            border: none;
            border-bottom: 2px solid #b5ecff;
            color: #374151;
            background: transparent;
            font-size: 15px;
            font-weight: 600;
        }
        QPushButton#loginModeButton[selected="true"] {
            border-bottom: 4px solid #3eceff;
            color: #3eceff;
            font-weight: 700;
        }
        QPushButton#authcodeBtn {
            border: 1px solid #dbe7f0;
            border-radius: 17px;
            color: #3eceff;
            background: #ffffff;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton#authcodeBtn:hover {
            border-color: #3eceff;
            background: #f3fbff;
        }
        QPushButton#loginBtn {
            border: none;
            border-radius: 20px;
            color: #ffffff;
            background: #3eceff;
            font-size: 15px;
            font-weight: 700;
        }
        QPushButton#loginBtn:hover {
            background: #27bee8;
        }
        QPushButton#registerBtn {
            border: none;
            color: #3eceff;
            background: transparent;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton#registerBtn:hover {
            color: #1599c4;
        }
        QPushButton#minBtn {
            border: none;
            border-image: url(:/images/login/suoxiao.png);
        }
        QPushButton#quitBtn {
            border: none;
            border-image: url(:/images/login/quxiao.png);
        }
    )");

    switchMode(Mode::Email);
}

void Login::onLoginButtonClicked()
{
    if (m_isLoginRequesting) {
        return;
    }

    if (m_mode == Mode::Email) {
        const QString email = m_emailEdit->text().trimmed();
        const QString authcode = m_authcodeEdit->text().trimmed();

        if (email.isEmpty()) {
            QMessageBox::warning(this, "邮箱登录", "邮箱不能为空");
            m_emailEdit->setFocus();
            return;
        }

        if (!isEmailValid(email)) {
            QMessageBox::warning(this, "邮箱登录", "邮箱格式错误");
            m_emailEdit->setFocus();
            return;
        }

        if (authcode.isEmpty()) {
            QMessageBox::warning(this, "邮箱登录", "验证码不能为空");
            m_authcodeEdit->setFocus();
            return;
        }

        if (authcode.length() != 6) {
            QMessageBox::warning(this, "邮箱登录", "验证码格式错误");
            m_authcodeEdit->setFocus();
            return;
        }

        if (m_authcodeId.isEmpty() || email != m_authcodeEmail || authcode != m_authcodeValue) {
            QMessageBox::warning(this, "邮箱登录", "验证码错误或已失效，请重新获取");
            m_authcodeEdit->setFocus();
            return;
        }

        auto &users = accountStore();
        if (!users.contains(email)) {
            users.insert(email, {nicknameFromEmail(email), {}});
            LOG() << "本地邮箱注册成功，邮箱:" << email;
        }

        const AccountInfo user = users.value(email);
        LOG() << "本地邮箱登录成功，邮箱:" << email << "昵称:" << user.nickname;
        emit loginSuccess(user.nickname, email);
        close();
        return;
    }

    const QString account = ui->accountEdit->text().trimmed();
    const QString password = ui->passwordEdit->text();

    const QString accountError = accountRuleError(account);
    if (!accountError.isEmpty()) {
        QMessageBox::warning(this, "密码登录", accountError);
        ui->accountEdit->setFocus();
        return;
    }

    const QString passwordError = passwordRuleError(password);
    if (!passwordError.isEmpty()) {
        QMessageBox::warning(this, "密码登录", passwordError);
        ui->passwordEdit->setFocus();
        return;
    }

    // 这是什么：把密码登录从本地账号表切到临时登录接口。
    // 为什么能实现：ApiClient::login() 会异步 POST /login，并通过 loginSucceeded/loginFailed 返回结果。
    // 什么时候调用：账号和密码的前端基础格式校验都通过后调用。
    // 和谁配合：mock server 负责返回临时用户信息，onLoginSucceeded/onLoginFailed 负责更新界面状态。
    m_isLoginRequesting = true;
    ui->loginBtn->setEnabled(false);
    ui->loginBtn->setText("登录中...");
    m_apiClient->login(account, password);
}

void Login::onLoginSucceeded(const QString &userName, const QString &account)
{
    // 这是什么：处理临时登录接口成功结果。
    // 为什么能实现：ApiClient 已经确认响应 success=true，并把 userName/account 从 JSON 中解析出来。
    // 什么时候调用：ApiClient::loginSucceeded 信号触发时由 Qt 自动调用。
    // 和谁配合：继续发已有 loginSuccess 信号，让 player.cpp 不用改也能更新“我的”页面。
    m_isLoginRequesting = false;
    ui->loginBtn->setEnabled(true);
    LOG() << "接口登录成功，账号:" << account << "昵称:" << userName;
    emit loginSuccess(userName, account);
    close();
}

void Login::onLoginFailed(const QString &message)
{
    // 这是什么：处理临时登录接口失败结果。
    // 为什么能实现：ApiClient 会把网络错误、响应格式错误和业务失败都统一转成 message。
    // 什么时候调用：ApiClient::loginFailed 信号触发时由 Qt 自动调用。
    // 和谁配合：登录按钮恢复可点，QMessageBox 把失败原因展示给用户。
    m_isLoginRequesting = false;
    ui->loginBtn->setEnabled(true);
    ui->loginBtn->setText(m_mode == Mode::Email ? "登录/注册" : "登录");
    QMessageBox::warning(this, "密码登录", message);
    ui->passwordEdit->setFocus();
}

void Login::onRegisterButtonClicked()
{
    switchMode(Mode::Email);
    m_emailEdit->setFocus();
    LOG() << "点击立即注册，切换到邮箱登录/注册入口";
}

void Login::onAuthcodeButtonClicked()
{
    const QString email = m_emailEdit->text().trimmed();
    if (email.isEmpty()) {
        QMessageBox::warning(this, "获取验证码", "邮箱不能为空");
        m_emailEdit->setFocus();
        return;
    }

    if (!isEmailValid(email)) {
        QMessageBox::warning(this, "获取验证码", "邮箱格式错误");
        m_emailEdit->setFocus();
        return;
    }

    m_authcodeEmail = email;
    m_authcodeValue = makeAuthcode();
    m_authcodeId = QString("local-%1").arg(QRandomGenerator::global()->generate());
    m_authcodeEdit->clear();
    m_authcodeEdit->setFocus();

    LOG() << "本地验证码已生成:" << email << m_authcodeValue << m_authcodeId;
    QMessageBox::information(this, "获取验证码", "本地验证码：" + m_authcodeValue);
}

void Login::switchMode(Mode mode)
{
    m_mode = mode;
    const bool isEmailMode = m_mode == Mode::Email;

    setFixedSize(430, 560);
    ui->titleLabel->setText(isEmailMode ? "邮箱登录" : "密码登录");
    ui->subTitleLabel->setText(isEmailMode ? "输入邮箱验证码即可登录或注册"
                                           : "使用邮箱或用户昵称和密码登录");
    ui->loginBtn->setText(isEmailMode ? "登录/注册" : "登录");
    ui->registerHintLabel->setText("还没有账号？");
    ui->registerBtn->setText("立即注册");

    ui->accountLabel->setVisible(!isEmailMode);
    ui->accountEdit->setVisible(!isEmailMode);
    ui->passwordLabel->setVisible(!isEmailMode);
    ui->passwordEdit->setVisible(!isEmailMode);

    m_emailLabel->setVisible(isEmailMode);
    m_emailEdit->setVisible(isEmailMode);
    m_authcodeBtn->setVisible(isEmailMode);
    m_authcodeLabel->setVisible(isEmailMode);
    m_authcodeEdit->setVisible(isEmailMode);

    clearInputs();
    refreshModeButtons();

    if (isEmailMode) {
        m_emailEdit->setFocus();
    } else {
        ui->accountEdit->setFocus();
    }

    LOG() << "登录窗口切换模式:" << (isEmailMode ? "邮箱登录" : "密码登录");
}

void Login::clearInputs()
{
    ui->accountEdit->clear();
    ui->passwordEdit->clear();
    m_emailEdit->clear();
    m_authcodeEdit->clear();
    m_authcodeId.clear();
    m_authcodeValue.clear();
    m_authcodeEmail.clear();
}

void Login::refreshModeButtons()
{
    const bool isEmailMode = m_mode == Mode::Email;
    m_passwordModeBtn->setProperty("selected", !isEmailMode);
    m_emailModeBtn->setProperty("selected", isEmailMode);

    for (auto *button : {m_passwordModeBtn, m_emailModeBtn}) {
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }
}
