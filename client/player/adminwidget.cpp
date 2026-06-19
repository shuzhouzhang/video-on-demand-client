// adminwidget.cpp implements the review and role-management API console.
#include "adminwidget.h"
#include "apiclient.h"
#include "ui_adminwidget.h"
#include "util.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <utility>

namespace {
constexpr int kActionColumn = 5;

QTableWidgetItem *readonlyItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

int pageCount(int total, int pageSize)
{
    if (total <= 0 || pageSize <= 0) {
        return 1;
    }

    return (total + pageSize - 1) / pageSize;
}
}

AdminWidget::AdminWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AdminWidget)
{
    initUI();
}

AdminWidget::~AdminWidget()
{
    delete ui;
}

void AdminWidget::initUI()
{
    ui->setupUi(this);
    m_apiClient = new ApiClient(this);

    ui->checkStatusCombo->addItems({"全部", "待审核", "审核通过", "审核拒绝"});
    ui->roleStatusCombo->addItems({"全部", "普通用户", "管理员", "超级管理员", "禁用"});

    setupCheckTable();
    setupRoleTable();
    setupPagination(ui->checkPageLayout, m_checkPageState, true);
    setupPagination(ui->rolePageLayout, m_rolePageState, false);
    applyCheckFilter();
    applyRoleFilter();
    switchAdminPage(0);

    connect(ui->checkBtn, &QPushButton::clicked, this, [this]() {
        switchAdminPage(0);
    });
    connect(ui->roleBtn, &QPushButton::clicked, this, [this]() {
        switchAdminPage(1);
    });

    connect(ui->checkQueryBtn, &QPushButton::clicked, this, [this]() {
        applyCheckFilter();
        LOG() << "后台审核查询:" << ui->checkUserIdEdit->text() << ui->checkStatusCombo->currentText();
    });
    connect(ui->checkResetBtn, &QPushButton::clicked, this, [this]() {
        ui->checkUserIdEdit->clear();
        ui->checkStatusCombo->setCurrentIndex(0);
        applyCheckFilter();
        LOG() << "重置审核管理筛选条件";
    });
    connect(ui->roleQueryBtn, &QPushButton::clicked, this, [this]() {
        applyRoleFilter();
        LOG() << "后台角色查询:" << ui->roleEmailEdit->text() << ui->roleStatusCombo->currentText();
    });
    connect(ui->roleResetBtn, &QPushButton::clicked, this, [this]() {
        ui->roleEmailEdit->clear();
        ui->roleStatusCombo->setCurrentIndex(0);
        applyRoleFilter();
        LOG() << "重置角色管理筛选条件";
    });
    connect(ui->addAdminBtn, &QPushButton::clicked, this, [this]() {
        // 这是什么：收集要提升为管理员的账号并提交接口。
        // 为什么能实现：QInputDialog 提供最小输入窗口，后端负责判断账号存在和角色变化。
        // 什么时候调用：点击“添加管理员”按钮时调用。
        // 和谁配合：ApiClient::updateAdminUser() 成功后刷新角色列表。
        bool accepted = false;
        const QString account = QInputDialog::getText(this,
                                                       QStringLiteral("添加管理员"),
                                                       QStringLiteral("用户账号"),
                                                       QLineEdit::Normal,
                                                       QString(),
                                                       &accepted).trimmed();
        if (accepted && !account.isEmpty()) {
            m_apiClient->updateAdminUser(account, "set-admin");
        }
    });

    connect(m_apiClient, &ApiClient::adminReviewsLoaded, this, &AdminWidget::setAdminReviews);
    connect(m_apiClient, &ApiClient::adminUsersLoaded, this, &AdminWidget::setAdminUsers);
    connect(m_apiClient, &ApiClient::adminActionSucceeded, this, [this](const QString &target) {
        if (target == "reviews") {
            m_apiClient->fetchAdminReviews();
        } else {
            m_apiClient->fetchAdminUsers();
        }
    });
    connect(m_apiClient, &ApiClient::adminRequestFailed, this, [this](const QString &message) {
        QMessageBox::warning(this, QStringLiteral("后台操作"), message);
        LOG() << "后台接口失败:" << message;
    });
    m_apiClient->fetchAdminReviews();
    m_apiClient->fetchAdminUsers();

    setStyleSheet(R"(
        QWidget#AdminWidget,
        QWidget#backgroundWidget {
            background: #f5f6f8;
        }
        QWidget#container,
        QWidget#checkPage,
        QWidget#rolePage {
            background: #ffffff;
        }
        QWidget#container {
            border-radius: 8px;
        }
        QFrame#tabBox {
            border: none;
            border-bottom: 2px solid #f5f6f8;
            background: #ffffff;
        }
        QPushButton#checkBtn,
        QPushButton#roleBtn {
            border: none;
            border-bottom: 2px solid #f5f6f8;
            background: #ffffff;
            color: #666666;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton#checkBtn[selected="true"],
        QPushButton#roleBtn[selected="true"] {
            color: #111827;
            border-bottom-color: #3eceff;
        }
        QWidget#checkFilterBox,
        QWidget#roleFilterBox {
            background: #f7f8fa;
            border-radius: 8px;
        }
        QLabel {
            color: #333333;
            font-size: 14px;
            background: transparent;
        }
        QLineEdit,
        QComboBox {
            min-height: 36px;
            border: 1px solid #dcdee0;
            border-radius: 4px;
            background: #ffffff;
            color: #222222;
            padding-left: 12px;
            padding-right: 8px;
        }
        QPushButton#checkQueryBtn,
        QPushButton#roleQueryBtn,
        QPushButton#addAdminBtn {
            border: none;
            border-radius: 18px;
            color: #ffffff;
            background: #3eceff;
            font-size: 14px;
            font-weight: 700;
        }
        QPushButton#checkResetBtn,
        QPushButton#roleResetBtn {
            border: 1px solid #dbe7f0;
            border-radius: 18px;
            color: #374151;
            background: #ffffff;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton#adminActionButton {
            border: 1px solid #dbe7f0;
            border-radius: 14px;
            color: #374151;
            background: #ffffff;
            font-size: 13px;
        }
        QPushButton#adminPageButton,
        QPushButton#adminPageArrowButton {
            min-width: 32px;
            min-height: 32px;
            border: 1px solid #dbe7f0;
            border-radius: 16px;
            color: #374151;
            background: #ffffff;
            font-size: 13px;
            font-weight: 600;
        }
        QPushButton#adminPageButton[selected="true"] {
            border-color: #3eceff;
            color: #ffffff;
            background: #3eceff;
        }
        QPushButton#adminPageButton:hover,
        QPushButton#adminPageArrowButton:hover {
            border-color: #3eceff;
            color: #0ea5d7;
            background: #f3fbff;
        }
        QPushButton#adminPageButton:disabled,
        QPushButton#adminPageArrowButton:disabled {
            border-color: #eef2f7;
            color: #c8d0d8;
            background: #f7f8fa;
        }
        QPushButton#adminActionButton:hover,
        QPushButton#checkResetBtn:hover,
        QPushButton#roleResetBtn:hover {
            border-color: #3eceff;
            color: #0ea5d7;
            background: #f3fbff;
        }
        QTableWidget {
            border: 1px solid #eef2f7;
            border-radius: 6px;
            gridline-color: #eef2f7;
            background: #ffffff;
            alternate-background-color: #fbfdff;
            color: #222222;
            font-size: 13px;
        }
        QHeaderView::section {
            height: 38px;
            border: none;
            border-bottom: 1px solid #eef2f7;
            background: #f7f8fa;
            color: #606a78;
            font-size: 13px;
            font-weight: 700;
        }
    )");
}

void AdminWidget::switchAdminPage(int index)
{
    ui->adminStackedWidget->setCurrentIndex(index);
    ui->checkBtn->setProperty("selected", index == 0);
    ui->roleBtn->setProperty("selected", index == 1);

    for (auto *button : {ui->checkBtn, ui->roleBtn}) {
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }

    LOG() << "切换后台页签:" << (index == 0 ? "审核管理" : "角色管理");
}

void AdminWidget::setupCheckTable()
{
    ui->checkTable->setColumnCount(6);
    ui->checkTable->setHorizontalHeaderLabels({"封面", "标题", "用户 ID", "状态", "上传时间", "操作"});
    ui->checkTable->setRowCount(0);
    ui->checkTable->setAlternatingRowColors(true);
    ui->checkTable->verticalHeader()->setVisible(false);
    ui->checkTable->horizontalHeader()->setStretchLastSection(true);
    ui->checkTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->checkTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->checkTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_checkPageState.sourceRows = {
        {{"默认封面", "北京旅游攻略第一版", "user-001", "待审核", "2026-05-31 09:30"}, {"通过", "拒绝", "查看"}, "北京旅游攻略第一版"},
        {{"默认封面", "美食测评：胡同小店", "user-018", "审核通过", "2026-05-30 18:20"}, {"通过", "拒绝", "查看"}, "美食测评：胡同小店"},
        {{"默认封面", "游戏攻略：新手路线", "user-026", "审核拒绝", "2026-05-29 12:45"}, {"通过", "拒绝", "查看"}, "游戏攻略：新手路线"},
        {{"默认封面", "科技观察：智能设备趋势", "user-032", "待审核", "2026-05-28 16:15"}, {"通过", "拒绝", "查看"}, "科技观察：智能设备趋势"},
        {{"默认封面", "运动训练：十分钟拉伸", "user-045", "审核通过", "2026-05-27 11:05"}, {"通过", "拒绝", "查看"}, "运动训练：十分钟拉伸"},
        {{"默认封面", "动物世界：森林的一天", "user-052", "待审核", "2026-05-26 20:40"}, {"通过", "拒绝", "查看"}, "动物世界：森林的一天"},
        {{"默认封面", "电影解说：经典桥段", "user-067", "审核拒绝", "2026-05-25 13:10"}, {"通过", "拒绝", "查看"}, "电影解说：经典桥段"},
        {{"默认封面", "世界史入门：文明起源", "user-081", "审核通过", "2026-05-24 09:25"}, {"通过", "拒绝", "查看"}, "世界史入门：文明起源"},
        {{"默认封面", "摄影教程：夜景构图", "user-094", "待审核", "2026-05-23 17:55"}, {"通过", "拒绝", "查看"}, "摄影教程：夜景构图"},
        {{"默认封面", "音乐现场：校园乐队", "user-108", "审核通过", "2026-05-22 19:30"}, {"通过", "拒绝", "查看"}, "音乐现场：校园乐队"},
        {{"默认封面", "生活记录：周末整理", "user-116", "审核拒绝", "2026-05-21 08:50"}, {"通过", "拒绝", "查看"}, "生活记录：周末整理"},
        {{"默认封面", "编程入门：Qt 控件布局", "user-128", "待审核", "2026-05-20 15:35"}, {"通过", "拒绝", "查看"}, "编程入门：Qt 控件布局"},
    };
}

void AdminWidget::setupRoleTable()
{
    ui->roleTable->setColumnCount(6);
    ui->roleTable->setHorizontalHeaderLabels({"邮箱", "昵称", "角色", "状态", "创建时间", "操作"});
    ui->roleTable->setRowCount(0);
    ui->roleTable->setAlternatingRowColors(true);
    ui->roleTable->verticalHeader()->setVisible(false);
    ui->roleTable->horizontalHeader()->setStretchLastSection(true);
    ui->roleTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->roleTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->roleTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_rolePageState.sourceRows = {
        {{"admin@bit.com", "系统管理员", "超级管理员", "启用", "2026-05-01 10:00"}, {"设为管理员", "禁用", "删除"}, "admin@bit.com"},
        {{"review@bit.com", "审核员", "管理员", "启用", "2026-05-12 14:30"}, {"设为管理员", "禁用", "删除"}, "review@bit.com"},
        {{"user001@bit.com", "普通用户一号", "普通用户", "禁用", "2026-05-20 08:10"}, {"设为管理员", "禁用", "删除"}, "user001@bit.com"},
        {{"travel@bit.com", "旅行作者", "普通用户", "启用", "2026-05-21 11:15"}, {"设为管理员", "禁用", "删除"}, "travel@bit.com"},
        {{"food@bit.com", "美食作者", "普通用户", "启用", "2026-05-22 16:40"}, {"设为管理员", "禁用", "删除"}, "food@bit.com"},
        {{"game@bit.com", "游戏作者", "管理员", "启用", "2026-05-23 09:05"}, {"设为管理员", "禁用", "删除"}, "game@bit.com"},
        {{"music@bit.com", "音乐作者", "普通用户", "禁用", "2026-05-24 13:25"}, {"设为管理员", "启用", "删除"}, "music@bit.com"},
        {{"sport@bit.com", "运动作者", "普通用户", "启用", "2026-05-25 10:50"}, {"设为管理员", "禁用", "删除"}, "sport@bit.com"},
        {{"movie@bit.com", "电影作者", "管理员", "启用", "2026-05-26 18:35"}, {"设为管理员", "禁用", "删除"}, "movie@bit.com"},
        {{"tech@bit.com", "科技作者", "普通用户", "启用", "2026-05-27 14:20"}, {"设为管理员", "禁用", "删除"}, "tech@bit.com"},
        {{"photo@bit.com", "摄影作者", "普通用户", "禁用", "2026-05-28 07:45"}, {"设为管理员", "启用", "删除"}, "photo@bit.com"},
        {{"ops@bit.com", "运营同学", "超级管理员", "启用", "2026-05-29 12:00"}, {"设为管理员", "禁用", "删除"}, "ops@bit.com"},
    };
}

void AdminWidget::setupPagination(QVBoxLayout *pageLayout, PageState &state, bool isCheckPage)
{
    auto *paginationWidget = new QWidget(this);
    paginationWidget->setObjectName("adminPaginationBar");
    paginationWidget->setFixedHeight(42);
    paginationWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *layout = new QHBoxLayout(paginationWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    state.totalLabel = new QLabel(paginationWidget);
    state.totalLabel->setObjectName("adminPageTotalLabel");
    state.totalLabel->setMinimumWidth(120);

    state.prevButton = new QPushButton(paginationWidget);
    state.prevButton->setObjectName("adminPageArrowButton");
    state.prevButton->setCursor(Qt::PointingHandCursor);
    state.prevButton->setIcon(QIcon(":/images/admin/arrow-left.png"));
    state.prevButton->setIconSize(QSize(16, 16));
    state.prevButton->setFixedSize(32, 32);

    state.nextButton = new QPushButton(paginationWidget);
    state.nextButton->setObjectName("adminPageArrowButton");
    state.nextButton->setCursor(Qt::PointingHandCursor);
    state.nextButton->setIcon(QIcon(":/images/admin/arrow-right.png"));
    state.nextButton->setIconSize(QSize(16, 16));
    state.nextButton->setFixedSize(32, 32);

    auto *pageButtonsWidget = new QWidget(paginationWidget);
    state.pageButtonLayout = new QHBoxLayout(pageButtonsWidget);
    state.pageButtonLayout->setContentsMargins(0, 0, 0, 0);
    state.pageButtonLayout->setSpacing(6);

    layout->addWidget(state.totalLabel);
    layout->addStretch();
    layout->addWidget(state.prevButton);
    layout->addWidget(pageButtonsWidget);
    layout->addWidget(state.nextButton);

    connect(state.prevButton, &QPushButton::clicked, this, [this, isCheckPage]() {
        PageState &state = isCheckPage ? m_checkPageState : m_rolePageState;
        switchPage(state, isCheckPage, state.currentPage - 1);
    });
    connect(state.nextButton, &QPushButton::clicked, this, [this, isCheckPage]() {
        PageState &state = isCheckPage ? m_checkPageState : m_rolePageState;
        switchPage(state, isCheckPage, state.currentPage + 1);
    });

    pageLayout->addWidget(paginationWidget);
}

void AdminWidget::applyCheckFilter()
{
    const QString userId = ui->checkUserIdEdit->text().trimmed();
    const QString status = ui->checkStatusCombo->currentText();

    m_checkPageState.filteredRows.clear();
    for (const TableRow &row : std::as_const(m_checkPageState.sourceRows)) {
        const bool matchesUser = userId.isEmpty() || row.cells.value(2).contains(userId, Qt::CaseInsensitive);
        const bool matchesStatus = status == "全部" || row.cells.value(3) == status;
        if (matchesUser && matchesStatus) {
            m_checkPageState.filteredRows.append(row);
        }
    }

    m_checkPageState.currentPage = 1;
    renderCheckPage();
}

void AdminWidget::applyRoleFilter()
{
    const QString email = ui->roleEmailEdit->text().trimmed();
    const QString status = ui->roleStatusCombo->currentText();

    m_rolePageState.filteredRows.clear();
    for (const TableRow &row : std::as_const(m_rolePageState.sourceRows)) {
        const bool matchesEmail = email.isEmpty() || row.cells.value(0).contains(email, Qt::CaseInsensitive);
        const bool matchesStatus = status == "全部" || row.cells.value(2) == status || row.cells.value(3) == status;
        if (matchesEmail && matchesStatus) {
            m_rolePageState.filteredRows.append(row);
        }
    }

    m_rolePageState.currentPage = 1;
    renderRolePage();
}

void AdminWidget::renderCheckPage()
{
    renderTablePage(ui->checkTable, m_checkPageState);
    updatePagination(m_checkPageState, true);
}

void AdminWidget::renderRolePage()
{
    renderTablePage(ui->roleTable, m_rolePageState);
    updatePagination(m_rolePageState, false);
}

void AdminWidget::renderTablePage(QTableWidget *table, PageState &state)
{
    const int total = state.filteredRows.size();
    const int totalPages = pageCount(total, state.pageSize);
    state.currentPage = qBound(1, state.currentPage, totalPages);
    const int start = (state.currentPage - 1) * state.pageSize;
    const int count = qMin(state.pageSize, total - start);

    table->clearContents();
    table->setRowCount(qMax(0, count));

    for (int row = 0; row < count; ++row) {
        const TableRow &data = state.filteredRows[start + row];
        for (int column = 0; column < data.cells.size(); ++column) {
            table->setItem(row, column, readonlyItem(data.cells[column]));
        }
        appendActionButtons(table, row, data.actions, data.actionTarget);
        table->setRowHeight(row, 52);
    }
}

void AdminWidget::updatePagination(PageState &state, bool isCheckPage)
{
    const int total = state.filteredRows.size();
    const int totalPages = pageCount(total, state.pageSize);
    state.currentPage = qBound(1, state.currentPage, totalPages);

    state.totalLabel->setText(QString("共 %1 条").arg(total));
    state.prevButton->setEnabled(total > 0 && state.currentPage > 1);
    state.nextButton->setEnabled(total > 0 && state.currentPage < totalPages);

    qDeleteAll(state.pageButtons);
    state.pageButtons.clear();

    for (int page = 1; page <= totalPages; ++page) {
        auto *button = new QPushButton(QString::number(page));
        button->setObjectName("adminPageButton");
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedSize(32, 32);
        button->setEnabled(total > 0);
        button->setProperty("selected", page == state.currentPage);
        connect(button, &QPushButton::clicked, this, [this, isCheckPage, page]() {
            PageState &state = isCheckPage ? m_checkPageState : m_rolePageState;
            switchPage(state, isCheckPage, page);
        });

        state.pageButtonLayout->addWidget(button);
        state.pageButtons.append(button);
    }
}

void AdminWidget::switchPage(PageState &state, bool isCheckPage, int page)
{
    const int totalPages = pageCount(state.filteredRows.size(), state.pageSize);
    state.currentPage = qBound(1, page, totalPages);

    if (isCheckPage) {
        renderCheckPage();
        LOG() << "审核管理分页切换到第" << state.currentPage << "页";
        return;
    }

    renderRolePage();
    LOG() << "角色管理分页切换到第" << state.currentPage << "页";
}

void AdminWidget::appendActionButtons(QTableWidget *table, int row, const QStringList &actions, const QString &target)
{
    auto *box = new QWidget(table);
    auto *layout = new QHBoxLayout(box);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    for (const QString &action : actions) {
        auto *button = new QPushButton(action, box);
        button->setObjectName("adminActionButton");
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumHeight(28);
        connect(button, &QPushButton::clicked, this, [this, table, action, target]() {
            // 这是什么：把表格中文操作按钮转换成后台接口动作。
            // 为什么能实现：所在表格决定审核或角色业务，target 保存 videoId 或 account。
            // 什么时候调用：点击任意行操作按钮时调用。
            // 和谁配合：ApiClient POST 成功后重新加载对应表格。
            if (table == ui->checkTable) {
                if (action == "通过") {
                    m_apiClient->reviewVideo(target, "审核通过");
                } else if (action == "拒绝") {
                    m_apiClient->reviewVideo(target, "审核拒绝");
                }
            } else {
                QString apiAction;
                if (action == "设为管理员") apiAction = "set-admin";
                else if (action == "禁用") apiAction = "disable";
                else if (action == "启用") apiAction = "enable";
                else if (action == "删除") apiAction = "delete";
                if (!apiAction.isEmpty()) {
                    m_apiClient->updateAdminUser(target, apiAction);
                }
            }
        });
        layout->addWidget(button);
    }

    table->setCellWidget(row, kActionColumn, box);
}

void AdminWidget::setAdminReviews(const QList<AdminReviewInfo> &reviews)
{
    // 这是什么：把审核接口数据转换成表格内部行模型。
    // 为什么能实现：AdminReviewInfo 字段与审核表格前五列一一对应。
    // 什么时候调用：GET /admin/reviews 成功后调用。
    // 和谁配合：applyCheckFilter() 继续提供本地筛选和分页。
    m_checkPageState.sourceRows.clear();
    for (const AdminReviewInfo &review : reviews) {
        m_checkPageState.sourceRows.append({{"默认封面", review.title, review.userId, review.status, review.uploadTime},
                                             {"通过", "拒绝"},
                                             review.videoId});
    }
    applyCheckFilter();
}

void AdminWidget::setAdminUsers(const QList<AdminUserInfo> &users)
{
    // 这是什么：把角色接口数据转换成角色管理表格行模型。
    // 为什么能实现：状态决定显示“启用”或“禁用”，其它动作直接映射后端 action。
    // 什么时候调用：GET /admin/users 成功后调用。
    // 和谁配合：applyRoleFilter() 继续提供本地筛选和分页。
    m_rolePageState.sourceRows.clear();
    for (const AdminUserInfo &user : users) {
        const QString statusAction = user.status == "禁用" ? "启用" : "禁用";
        m_rolePageState.sourceRows.append({{user.account, user.userName, user.role, user.status, user.createdAt},
                                           {"设为管理员", statusAction, "删除"},
                                           user.account});
    }
    applyRoleFilter();
}
