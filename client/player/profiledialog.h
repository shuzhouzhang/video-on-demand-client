#ifndef PROFILEDIALOG_H
#define PROFILEDIALOG_H

#include <QDialog>

#include "datacenter.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;

// 这是什么：用于查看账号并编辑昵称、简介的个人资料窗口。
// 为什么这样做：把表单校验和提交状态从主窗口拆开，player.cpp 只负责协调接口和页面数据。
// 什么时候使用：已登录用户点击“编辑资料”时打开。
// 和谁配合：ApiClient 读写资料，DataCenter 保存结果，player.cpp 刷新“我的”页面。
class ProfileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProfileDialog(QWidget *parent = nullptr);
    void setUser(const UserInfo &user);
    void setSaving(bool saving);
    void showError(const QString &message);

signals:
    // 这是什么：资料表单校验通过后的保存请求。
    // 为什么能实现：信号携带昵称和简介，让窗口无需直接依赖网络层。
    // 什么时候触发：用户点击保存且长度合法时触发。
    // 和谁配合：player.cpp 接收后调用 ApiClient::updateUserProfile()。
    void saveRequested(const QString &userName, const QString &description);

private:
    QLabel *m_accountLabel = nullptr;
    QLineEdit *m_userNameEdit = nullptr;
    QTextEdit *m_descriptionEdit = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_errorLabel = nullptr;
    QPushButton *m_saveButton = nullptr;
};

#endif // PROFILEDIALOG_H
