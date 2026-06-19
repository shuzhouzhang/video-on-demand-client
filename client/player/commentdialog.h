#ifndef COMMENTDIALOG_H
#define COMMENTDIALOG_H

#include <QDialog>
#include <QList>

#include "datacenter.h"

class QLabel;
class QListWidget;
class QPushButton;
class QTextEdit;

// 这是什么：播放页用于查看和发表视频评论的独立窗口。
// 为什么这样做：把评论列表渲染和输入状态从 PlayerPage 拆开，避免播放器同时承担过多界面职责。
// 什么时候使用：用户点击播放页“评论”按钮时打开。
// 和谁配合：PlayerPage 负责业务协调，ApiClient 负责网络请求，CommentDialog 只管理评论界面。
class CommentDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CommentDialog(QWidget *parent = nullptr);

    // 这是什么：用接口返回的数据整体刷新评论列表。
    // 为什么能实现：CommentInfo 已把网络 JSON 转成界面需要的固定字段，可以直接逐条渲染。
    // 什么时候调用：GET /videos/comments 成功后调用。
    // 和谁配合：ApiClient 发 commentsLoaded，PlayerPage 把列表转交给本函数。
    void setComments(const QList<CommentInfo> &comments);

    // 这是什么：把刚发表成功的评论插到列表顶部。
    // 为什么能实现：POST 响应已经带回完整 CommentInfo，无需重新请求全部评论。
    // 什么时候调用：commentSent 信号触发后调用。
    // 和谁配合：ApiClient 保存评论，PlayerPage 转交结果，本窗口完成即时刷新。
    void prependComment(const CommentInfo &comment);

    // 这是什么：切换评论列表的加载状态。
    // 为什么能实现：状态文字能在异步请求期间给用户明确反馈。
    // 什么时候调用：打开窗口发请求前设为 true，收到成功或失败后设为 false。
    // 和谁配合：PlayerPage 控制请求生命周期，CommentDialog 负责展示状态。
    void setLoading(bool loading);

    // 这是什么：切换发表评论按钮的提交状态。
    // 为什么能实现：提交期间禁用输入和按钮，可避免用户连续发送重复请求。
    // 什么时候调用：发送请求前设为 true，成功或失败后设为 false。
    // 和谁配合：ApiClient 的 commentSent/commentRequestFailed 决定何时恢复。
    void setSubmitting(bool submitting);

    // 这是什么：在评论窗口内展示非阻塞错误信息。
    // 为什么这样做：评论失败不应打断视频播放，也不需要连续弹出模态警告框。
    // 什么时候调用：评论列表读取或评论发送失败时调用。
    // 和谁配合：ApiClient 提供错误 message，PlayerPage 转交给本窗口。
    void showError(const QString &message);

signals:
    // 这是什么：用户确认发表评论后的界面事件。
    // 为什么能实现：信号只传正文，让 PlayerPage 决定登录规则和调用哪个接口。
    // 什么时候触发：用户输入 1 到 200 个字符并点击发送时触发。
    // 和谁配合：PlayerPage 接收后调用 ApiClient::sendComment()。
    void submitRequested(const QString &content);

private:
    void renderComments();
    void updateCharacterCount();

    QList<CommentInfo> m_comments;
    QLabel *m_statusLabel = nullptr;
    QListWidget *m_commentList = nullptr;
    QTextEdit *m_commentEdit = nullptr;
    QLabel *m_characterCountLabel = nullptr;
    QPushButton *m_sendButton = nullptr;
};

#endif // COMMENTDIALOG_H
