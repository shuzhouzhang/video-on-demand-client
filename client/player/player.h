// player.h 声明主窗口类。
// 主窗口负责承载顶部栏、左侧导航、右侧页面栈，并处理无边框窗口拖拽。
#ifndef PLAYER_H
#define PLAYER_H

#include <QList>
#include <QPoint>
#include <QString>
#include <QWidget>

#include "datacenter.h"

class ApiClient;
class Login;
class ProfileDialog;
class QEvent;
class QGridLayout;
class QLayout;
class QMouseEvent;
class QObject;
class QPushButton;

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
    void initHomeFilters();
    void refreshHomeCategoryButtons();
    void refreshHomeTagButtons();
    void renderHomeVideos();
    void setHomeVideos(const QList<VideoInfo> &videos);
    void searchHomeVideos();
    void renderMyVideoList(const QList<VideoInfo> &videos, const QString &title, const QString &emptyText);
    void selectHomeCategory(const QString &category);
    void clearLayout(QLayout *layout);
    void showLoginWindow();
    void updateLoginState(const QString &userName, const QString &account);
    void applyUserProfile(const UserInfo &user);

private:
    // ui 指向 Qt Designer 生成的界面对象，控件都从这里访问。
    Ui::player *ui;

    // true 表示用户正在按住窗口拖动。
    bool m_isDragging = false;

    // 记录鼠标点下去的位置到窗口左上角的距离。
    // 窗口移动时要靠它计算新位置。
    QPoint m_dragOffset;

    Login *m_loginWindow = nullptr;
    ApiClient *m_apiClient = nullptr;
    ProfileDialog *m_profileDialog = nullptr;
    QList<VideoInfo> m_homeVideos;
    QString m_selectedCategory;
    QString m_selectedTag;
    QString m_searchKeyword;
    QList<QPushButton *> m_categoryButtons;
    QList<QPushButton *> m_tagButtons;
    QGridLayout *m_myVideoGridLayout = nullptr;
};

#endif // PLAYER_H
