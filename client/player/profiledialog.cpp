#include "profiledialog.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

ProfileDialog::ProfileDialog(QWidget *parent)
    : QDialog(parent)
{
    // 这是什么：构建个人资料编辑表单。
    // 为什么能实现：账号只读，昵称和简介可编辑，保存前在客户端做长度校验。
    // 什么时候调用：player 主窗口第一次创建资料窗口时调用一次。
    // 和谁配合：setUser() 填充 DataCenter 当前值，saveRequested() 把修改交给 ApiClient。
    setWindowTitle(QStringLiteral("编辑个人资料"));
    setModal(true);
    setFixedSize(520, 360);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("编辑个人资料"), this);
    title->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 700;"));
    root->addWidget(title);

    auto *form = new QFormLayout;
    form->setSpacing(12);
    m_accountLabel = new QLabel(this);
    m_userNameEdit = new QLineEdit(this);
    m_userNameEdit->setMaxLength(20);
    m_descriptionEdit = new QTextEdit(this);
    m_descriptionEdit->setFixedHeight(100);
    form->addRow(QStringLiteral("账号"), m_accountLabel);
    form->addRow(QStringLiteral("昵称"), m_userNameEdit);
    form->addRow(QStringLiteral("简介"), m_descriptionEdit);
    root->addLayout(form);

    auto *bottom = new QHBoxLayout;
    m_countLabel = new QLabel(QStringLiteral("0/100"), this);
    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet(QStringLiteral("color: #dc2626;"));
    m_errorLabel->hide();
    bottom->addWidget(m_countLabel);
    bottom->addWidget(m_errorLabel, 1);
    m_saveButton = new QPushButton(QStringLiteral("保存"), this);
    m_saveButton->setFixedSize(80, 34);
    bottom->addWidget(m_saveButton);
    root->addLayout(bottom);

    setStyleSheet(R"(
        QDialog { background: #ffffff; color: #111827; }
        QLineEdit, QTextEdit { border: 1px solid #d9e0e7; border-radius: 6px; padding: 7px; background: #f8fafc; }
        QLineEdit:focus, QTextEdit:focus { border-color: #26bff3; background: #ffffff; }
        QPushButton { border: none; border-radius: 6px; background: #26bff3; color: white; font-weight: 600; }
        QPushButton:disabled { background: #b8dfee; }
    )");

    connect(m_descriptionEdit, &QTextEdit::textChanged, this, [this]() {
        QString text = m_descriptionEdit->toPlainText();
        if (text.size() > 100) {
            text.truncate(100);
            m_descriptionEdit->blockSignals(true);
            m_descriptionEdit->setPlainText(text);
            m_descriptionEdit->moveCursor(QTextCursor::End);
            m_descriptionEdit->blockSignals(false);
        }
        m_countLabel->setText(QStringLiteral("%1/100").arg(text.size()));
    });

    connect(m_saveButton, &QPushButton::clicked, this, [this]() {
        const QString userName = m_userNameEdit->text().trimmed();
        if (userName.isEmpty()) {
            showError(QStringLiteral("昵称不能为空"));
            return;
        }
        m_errorLabel->hide();
        emit saveRequested(userName, m_descriptionEdit->toPlainText().trimmed());
    });
}

void ProfileDialog::setUser(const UserInfo &user)
{
    // 这是什么：把当前用户资料填进编辑表单。
    // 为什么能实现：UserInfo 字段与账号标签、昵称输入框和简介输入框一一对应。
    // 什么时候调用：用户每次打开编辑资料窗口前调用。
    // 和谁配合：DataCenter 提供当前资料，用户修改后由 saveRequested 提交。
    m_accountLabel->setText(user.account);
    m_userNameEdit->setText(user.userName);
    m_descriptionEdit->setPlainText(user.description);
    m_errorLabel->hide();
    setSaving(false);
}

void ProfileDialog::setSaving(bool saving)
{
    // 这是什么：切换资料表单的保存中状态。
    // 为什么能实现：禁用输入和按钮可阻止同一份资料被重复提交。
    // 什么时候调用：POST 前设为 true，接口成功或失败后恢复 false。
    // 和谁配合：player.cpp 根据 ApiClient 信号控制本函数。
    m_userNameEdit->setEnabled(!saving);
    m_descriptionEdit->setEnabled(!saving);
    m_saveButton->setEnabled(!saving);
    m_saveButton->setText(saving ? QStringLiteral("保存中...") : QStringLiteral("保存"));
}

void ProfileDialog::showError(const QString &message)
{
    // 这是什么：在资料窗口中展示保存失败原因。
    // 为什么这样做：保留用户当前输入，允许修正后再次提交。
    // 什么时候调用：本地校验失败或 userProfileFailed 信号触发时调用。
    // 和谁配合：setSaving(false) 恢复表单，m_errorLabel 显示 ApiClient 的 message。
    setSaving(false);
    m_errorLabel->setText(message);
    m_errorLabel->show();
}
