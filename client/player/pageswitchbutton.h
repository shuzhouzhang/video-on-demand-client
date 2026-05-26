// pageswitchbutton.h 声明左侧导航按钮控件。
// 它不是普通 QPushButton，而是由“图标 QLabel + 文字 QLabel”组合成的自定义 QWidget。
#ifndef PAGESWITCHBUTTON_H
#define PAGESWITCHBUTTON_H

#include <QWidget>

class QLabel;
class QEnterEvent;
class QEvent;
class QMouseEvent;
class QPixmap;

class PageSwitchButton : public QWidget
{
    Q_OBJECT

public:
    explicit PageSwitchButton(QWidget *parent = nullptr);

    // 创建控件时，直接传入左侧图片和右侧文字。
    explicit PageSwitchButton(const QPixmap &icon, const QString &text, QWidget *parent = nullptr);

    // 修改左侧图标。
    void setIcon(const QPixmap &icon);

    // 修改右侧文字。
    void setText(const QString &text);

    // 获取当前显示的文字。
    QString text() const;

    // 设置当前导航按钮是否处于“选中”状态。
    void setChecked(bool checked);
    bool isChecked() const;

signals:
    // 点击这个自定义控件时，发出 clicked() 信号。
    void clicked();

protected:
    // 鼠标移进来时触发，用来做 hover 效果。
    void enterEvent(QEnterEvent *event) override;

    // 鼠标移出去时触发，用来取消 hover 效果。
    void leaveEvent(QEvent *event) override;

    // QWidget 本身不是按钮，所以这里自己处理鼠标点击。
    void mousePressEvent(QMouseEvent *event) override;

private:
    // 刷新 QSS 样式。checked/hovered 改变后要手动刷新，界面才会立刻变化。
    void refreshStyle();

    // 左边的 QLabel：专门显示图片。
    QLabel *m_iconLabel = nullptr;

    // 右边的 QLabel：专门显示文字。
    QLabel *m_textLabel = nullptr;

    bool m_checked = false;
    bool m_hovered = false;
};

#endif // PAGESWITCHBUTTON_H
