// login.cpp 实现登录窗口的静态登录流程。
// 当前阶段只做本地输入校验和登录成功信号，后续可在这里替换为真实登录接口。
#include "login.h"
#include "ui_login.h"
#include "util.h"

#include <QMessageBox>
#include <QMouseEvent>

namespace {
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
    ui->accountEdit->clear();
    ui->passwordEdit->clear();
    ui->accountEdit->setFocus();
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
    ui->passwordEdit->setEchoMode(QLineEdit::Password);

    ui->minBtn->setFlat(true);
    ui->quitBtn->setFlat(true);

    connect(ui->minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(ui->quitBtn, &QPushButton::clicked, this, &QWidget::close);
    connect(ui->loginBtn, &QPushButton::clicked, this, &Login::onLoginButtonClicked);
    connect(ui->registerBtn, &QPushButton::clicked, this, []() {
        LOG() << "点击立即注册，当前阶段仅作为占位";
    });

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
        QLabel#passwordLabel {
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
}

void Login::onLoginButtonClicked()
{
    const QString account = ui->accountEdit->text().trimmed();
    const QString password = ui->passwordEdit->text();

    if (account.isEmpty()) {
        QMessageBox::warning(this, "登录", "账号不能为空");
        ui->accountEdit->setFocus();
        return;
    }

    const QString passwordError = passwordRuleError(password);
    if (!passwordError.isEmpty()) {
        QMessageBox::warning(this, "登录", passwordError);
        ui->passwordEdit->setFocus();
        return;
    }

    LOG() << "静态登录成功，账号:" << account;
    emit loginSuccess(account, account);
    close();
}
