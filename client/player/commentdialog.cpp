#include "commentdialog.h"

#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>

CommentDialog::CommentDialog(QWidget *parent)
    : QDialog(parent)
{
    // 这是什么：创建评论窗口的标题、列表、输入框和发送按钮。
    // 为什么能实现：Qt 布局会稳定管理各控件尺寸，窗口可独立于播放页展示和关闭。
    // 什么时候调用：PlayerPage 第一次初始化评论功能时调用一次。
    // 和谁配合：后续 setComments() 填充列表，submitRequested() 把输入交回 PlayerPage。
    setWindowTitle(QStringLiteral("视频评论"));
    setModal(false);
    resize(720, 560);
    setMinimumSize(620, 480);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(24, 20, 24, 20);
    rootLayout->setSpacing(12);

    auto *titleLabel = new QLabel(QStringLiteral("评论"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    rootLayout->addWidget(titleLabel);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->hide();
    rootLayout->addWidget(m_statusLabel);

    m_commentList = new QListWidget(this);
    m_commentList->setFrameShape(QFrame::NoFrame);
    m_commentList->setSelectionMode(QAbstractItemView::NoSelection);
    m_commentList->setSpacing(4);
    rootLayout->addWidget(m_commentList, 1);

    m_commentEdit = new QTextEdit(this);
    m_commentEdit->setPlaceholderText(QStringLiteral("说点友善的话..."));
    m_commentEdit->setFixedHeight(82);
    rootLayout->addWidget(m_commentEdit);

    auto *actionLayout = new QHBoxLayout;
    m_characterCountLabel = new QLabel(QStringLiteral("0/200"), this);
    actionLayout->addWidget(m_characterCountLabel);
    actionLayout->addStretch(1);
    m_sendButton = new QPushButton(QStringLiteral("发送"), this);
    m_sendButton->setFixedSize(76, 34);
    actionLayout->addWidget(m_sendButton);
    rootLayout->addLayout(actionLayout);

    setStyleSheet(R"(
        QDialog { background: #ffffff; color: #111827; }
        QLabel { color: #111827; }
        QListWidget { background: #ffffff; outline: none; }
        QListWidget::item { border-bottom: 1px solid #edf0f3; padding: 4px 0; }
        QTextEdit {
            border: 1px solid #d9e0e7;
            border-radius: 6px;
            padding: 8px;
            background: #f8fafc;
            color: #111827;
        }
        QTextEdit:focus { border-color: #26bff3; background: #ffffff; }
        QPushButton {
            border: none;
            border-radius: 6px;
            background: #26bff3;
            color: #ffffff;
            font-weight: 600;
        }
        QPushButton:hover { background: #14aee2; }
        QPushButton:disabled { background: #b8dfee; color: #f5fbfd; }
    )");

    connect(m_commentEdit, &QTextEdit::textChanged, this, [this]() {
        // 这是什么：把评论输入长度稳定限制在 200 个字符内。
        // 为什么能实现：每次文本变化后截断超出部分，并把光标移回末尾。
        // 什么时候调用：用户输入、粘贴或程序修改评论正文时自动调用。
        // 和谁配合：updateCharacterCount() 同步显示剩余输入状态。
        QString content = m_commentEdit->toPlainText();
        if (content.size() > 200) {
            content.truncate(200);
            m_commentEdit->blockSignals(true);
            m_commentEdit->setPlainText(content);
            m_commentEdit->moveCursor(QTextCursor::End);
            m_commentEdit->blockSignals(false);
        }
        updateCharacterCount();
    });

    connect(m_sendButton, &QPushButton::clicked, this, [this]() {
        // 这是什么：评论窗口发送按钮的输入校验入口。
        // 为什么能实现：正文只有在去除首尾空白后非空，才允许交给业务层发送。
        // 什么时候调用：用户点击发送按钮时调用。
        // 和谁配合：submitRequested() 通知 PlayerPage 检查登录并调用 ApiClient。
        const QString content = m_commentEdit->toPlainText().trimmed();
        if (content.isEmpty()) {
            showError(QStringLiteral("评论内容不能为空"));
            return;
        }
        emit submitRequested(content);
    });

    renderComments();
}

void CommentDialog::setComments(const QList<CommentInfo> &comments)
{
    m_comments = comments;
    renderComments();
    setLoading(false);
}

void CommentDialog::prependComment(const CommentInfo &comment)
{
    m_comments.prepend(comment);
    renderComments();
    m_commentEdit->clear();
    setSubmitting(false);
    m_statusLabel->hide();
}

void CommentDialog::setLoading(bool loading)
{
    m_commentList->setEnabled(!loading);
    if (loading) {
        m_statusLabel->setText(QStringLiteral("正在加载评论..."));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #64748b;"));
        m_statusLabel->show();
    } else if (m_statusLabel->text() == QStringLiteral("正在加载评论...")) {
        m_statusLabel->hide();
    }
}

void CommentDialog::setSubmitting(bool submitting)
{
    m_commentEdit->setEnabled(!submitting);
    m_sendButton->setEnabled(!submitting);
    m_sendButton->setText(submitting ? QStringLiteral("发送中...") : QStringLiteral("发送"));
}

void CommentDialog::showError(const QString &message)
{
    setLoading(false);
    setSubmitting(false);
    m_statusLabel->setText(message);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #dc2626;"));
    m_statusLabel->show();
}

void CommentDialog::renderComments()
{
    // 这是什么：根据当前 m_comments 重建可见评论列表。
    // 为什么能实现：接口已按最新优先返回，按 QList 顺序逐条创建列表项即可保持顺序。
    // 什么时候调用：首次加载、重新加载或新评论发送成功后调用。
    // 和谁配合：setComments() 提供整批数据，prependComment() 提供最新一条数据。
    m_commentList->clear();
    if (m_comments.isEmpty()) {
        auto *emptyItem = new QListWidgetItem(QStringLiteral("还没有评论，来发表第一条吧"), m_commentList);
        emptyItem->setTextAlignment(Qt::AlignCenter);
        emptyItem->setFlags(Qt::NoItemFlags);
        emptyItem->setSizeHint(QSize(0, 72));
        return;
    }

    for (const CommentInfo &comment : m_comments) {
        auto *item = new QListWidgetItem(m_commentList);

        auto *container = new QWidget(m_commentList);
        auto *layout = new QVBoxLayout(container);
        layout->setContentsMargins(6, 8, 6, 8);
        layout->setSpacing(5);

        auto *metaLabel = new QLabel(
            QStringLiteral("%1  ·  %2").arg(comment.userName, comment.createdAt),
            container);
        metaLabel->setStyleSheet(QStringLiteral("color: #64748b; font-size: 12px;"));
        layout->addWidget(metaLabel);

        auto *contentLabel = new QLabel(comment.content, container);
        contentLabel->setWordWrap(true);
        contentLabel->setStyleSheet(QStringLiteral("color: #111827; font-size: 14px;"));
        layout->addWidget(contentLabel);
        const int availableTextWidth = qMax(420, m_commentList->viewport()->width() - 32);
        const int contentHeight = QFontMetrics(contentLabel->font())
                                      .boundingRect(QRect(0, 0, availableTextWidth, 1000),
                                                    Qt::TextWordWrap,
                                                    comment.content)
                                      .height();
        item->setSizeHint(QSize(0, qMax(82, contentHeight + 48)));
        m_commentList->setItemWidget(item, container);
    }
}

void CommentDialog::updateCharacterCount()
{
    m_characterCountLabel->setText(QStringLiteral("%1/200").arg(m_commentEdit->toPlainText().size()));
}
