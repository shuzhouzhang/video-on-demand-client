#ifndef PLAYER_H
#define PLAYER_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class player;
}
QT_END_NAMESPACE

class player : public QMainWindow
{
    Q_OBJECT

public:
    explicit player(QWidget *parent = nullptr);
    ~player() override;

private:
    Ui::player *ui;
};
#endif // PLAYER_H
