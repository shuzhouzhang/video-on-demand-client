// startuppage.h 声明启动页窗口。
// 启动页是主窗口出现前展示的无边框白底 logo 页面。
#ifndef STARTUPPAGE_H
#define STARTUPPAGE_H

#include <QDialog>

class StartupPage : public QDialog
{
    Q_OBJECT

public:
    explicit StartupPage(QWidget *parent = nullptr);
};

#endif // STARTUPPAGE_H
