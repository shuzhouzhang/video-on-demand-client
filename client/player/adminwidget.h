// adminwidget.h declares the static admin console page.
// AdminWidget follows the reference client's two-tab layout: review management and role management.
#ifndef ADMINWIDGET_H
#define ADMINWIDGET_H

#include <QList>
#include <QStringList>
#include <QWidget>

namespace Ui {
class AdminWidget;
}

class QHBoxLayout;
class QLabel;
class QPushButton;
class QTableWidget;
class QVBoxLayout;

class AdminWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AdminWidget(QWidget *parent = nullptr);
    ~AdminWidget() override;

private:
    struct TableRow {
        QStringList cells;
        QStringList actions;
        QString actionTarget;
    };

    struct PageState {
        QList<TableRow> sourceRows;
        QList<TableRow> filteredRows;
        int currentPage = 1;
        int pageSize = 5;
        QLabel *totalLabel = nullptr;
        QPushButton *prevButton = nullptr;
        QPushButton *nextButton = nullptr;
        QHBoxLayout *pageButtonLayout = nullptr;
        QList<QPushButton *> pageButtons;
    };

    void initUI();
    void switchAdminPage(int index);
    void setupCheckTable();
    void setupRoleTable();
    void setupPagination(QVBoxLayout *pageLayout, PageState &state, bool isCheckPage);
    void applyCheckFilter();
    void applyRoleFilter();
    void renderCheckPage();
    void renderRolePage();
    void renderTablePage(QTableWidget *table, PageState &state);
    void updatePagination(PageState &state, bool isCheckPage);
    void switchPage(PageState &state, bool isCheckPage, int page);
    void appendActionButtons(QTableWidget *table, int row, const QStringList &actions, const QString &target);

private:
    Ui::AdminWidget *ui;
    PageState m_checkPageState;
    PageState m_rolePageState;
};

#endif // ADMINWIDGET_H
