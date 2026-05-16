#include "MainWindow.h"
#include "ConnectDialog.h"

#include <QAction>
#include <QDockWidget>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <QInputDialog>
#include <QRegularExpression>

namespace
{
    constexpr int RoleType = Qt::UserRole;
    constexpr int RoleDatabase = Qt::UserRole + 1;
    constexpr int RoleTable = Qt::UserRole + 2;

    bool isSafeIdentifier(const QString &name)
    {
        static const QRegularExpression re("^[A-Za-z_][A-Za-z0-9_]*$");
        return re.match(name).hasMatch();
    }


    QString tableCellText(QTableWidget *table, int row, int col)
{
    QTableWidgetItem *item = table->item(row, col);
    return item ? item->text().trimmed() : "";
}


}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_requestMode(ClientRequestMode::None),
      m_dbClient(new DbClient(this)),
      m_saveTotalCount(0),
      m_isLoadingTable(false),
      m_hasUnsavedChanges(false),
      m_loginPending(false),
      m_isSavingToServer(false),
      m_isSavingTableChanges(false),
      m_isRefreshingAfterSave(false),
      m_isSavingStructure(false)

{

    m_connectionName = "本地连接";

    resize(1280, 820);

    createActions();
    createMenus();
    createToolBar();
    createCentralArea();
    createStatusBar();
    createLogDock();

    m_databaseTree->clear();

    auto *placeholderItem = new QTreeWidgetItem(m_databaseTree);
    placeholderItem->setText(0, "未连接服务端");
    placeholderItem->setData(0, RoleType, "placeholder");

    updateWindowCaption();


    connect(m_dbClient, &DbClient::connected, this, &MainWindow::onServerConnected);
    connect(m_dbClient, &DbClient::loginSucceeded, this, &MainWindow::onServerLoginSucceeded);
    connect(m_dbClient, &DbClient::responseReceived, this, &MainWindow::onServerResponse);
    connect(m_dbClient, &DbClient::errorReceived, this, &MainWindow::onServerError);
    connect(m_dbClient, &DbClient::disconnected, this, &MainWindow::onServerDisconnected);

    appendLog("客户端 UI 已启动。");
    appendLog("请通过“新建连接”连接服务端，然后在 SQL 编辑器中执行语句。");
    m_statusLabel->setText("就绪");
}

void MainWindow::createActions()
{
    m_newConnectionAction = new QAction("新建连接", this);
    m_refreshAction = new QAction("刷新", this);
    m_executeSqlAction = new QAction("执行 SQL", this);
    m_aboutAction = new QAction("关于", this);
    m_exitAction = new QAction("退出", this);
    m_createDatabaseAction = new QAction("新建数据库", this);
    m_deleteDatabaseAction = new QAction("删除数据库", this);
    m_createTableAction = new QAction("新建表", this);
    m_deleteTableAction = new QAction("删除表", this);

    connect(m_newConnectionAction, &QAction::triggered, this, &MainWindow::onNewConnection);
    connect(m_refreshAction, &QAction::triggered, this, &MainWindow::onRefresh);
    connect(m_executeSqlAction, &QAction::triggered, this, &MainWindow::onExecuteSql);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
    connect(m_createDatabaseAction, &QAction::triggered,this, &MainWindow::onCreateDatabase);
    connect(m_deleteDatabaseAction, &QAction::triggered,this, &MainWindow::onDeleteDatabase);
    connect(m_createTableAction, &QAction::triggered,this, &MainWindow::onCreateTable);
    connect(m_deleteTableAction, &QAction::triggered,this, &MainWindow::onDeleteTable);
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu("文件");
    fileMenu->addAction(m_newConnectionAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exitAction);

    QMenu *databaseMenu = menuBar()->addMenu("数据库");
    databaseMenu->addAction(m_createDatabaseAction);
    databaseMenu->addAction(m_deleteDatabaseAction);
    databaseMenu->addSeparator();
    databaseMenu->addAction(m_createTableAction);
    databaseMenu->addAction(m_deleteTableAction);
    databaseMenu->addSeparator();
    databaseMenu->addAction(m_refreshAction);

    QMenu *sqlMenu = menuBar()->addMenu("SQL");
    sqlMenu->addAction(m_executeSqlAction);

    QMenu *helpMenu = menuBar()->addMenu("帮助");
    helpMenu->addAction(m_aboutAction);
}

void MainWindow::createToolBar()
{
    QToolBar *toolBar = addToolBar("主工具栏");
    toolBar->setMovable(false);

    toolBar->addAction(m_newConnectionAction);
    toolBar->addAction(m_refreshAction);
    toolBar->addSeparator();
    toolBar->addAction(m_executeSqlAction);
}

void MainWindow::createCentralArea()
{
    auto *mainSplitter = new QSplitter(this);

    // 左侧：对象浏览器
    auto *leftPanel = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(8, 8, 8, 8);
    leftLayout->setSpacing(6);

    auto *leftTitle = new QLabel("对象浏览器", leftPanel);
    leftTitle->setObjectName("panelTitle");

    m_databaseTree = new QTreeWidget(leftPanel);
    m_databaseTree->setHeaderLabel("连接 / 数据库 / 表");
    m_databaseTree->setMinimumWidth(280);

    connect(m_databaseTree, &QTreeWidget::itemClicked,
            this, &MainWindow::onTreeItemClicked);

    leftLayout->addWidget(leftTitle);
    leftLayout->addWidget(m_databaseTree);

    // 右侧：Tab 区
    auto *rightPanel = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(8, 8, 8, 8);
    rightLayout->setSpacing(6);

    auto *rightTitle = new QLabel("工作区", rightPanel);
    rightTitle->setObjectName("panelTitle");

    m_tabWidget = new QTabWidget(rightPanel);

    // 1. 表数据页
    auto *dataPage = new QWidget(this);
    auto *dataLayout = new QVBoxLayout(dataPage);
    dataLayout->setContentsMargins(8, 8, 8, 8);
    dataLayout->setSpacing(8);

    auto *buttonLayout = new QHBoxLayout;
    m_addRowButton = new QPushButton("新增行", dataPage);
    m_deleteRowButton = new QPushButton("删除行", dataPage);
    m_saveButton = new QPushButton("保存修改", dataPage);
    m_refreshButton = new QPushButton("刷新当前表", dataPage);

    connect(m_addRowButton, &QPushButton::clicked, this, &MainWindow::onAddRow);
    connect(m_deleteRowButton, &QPushButton::clicked, this, &MainWindow::onDeleteRow);
    connect(m_saveButton, &QPushButton::clicked, this, &MainWindow::onSaveChanges);
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::onRefresh);

    buttonLayout->addWidget(m_addRowButton);
    buttonLayout->addWidget(m_deleteRowButton);
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addStretch();

    m_dataTable = new QTableWidget(dataPage);
    m_dataTable->setEditTriggers(QAbstractItemView::DoubleClicked
                                 | QAbstractItemView::EditKeyPressed
                                 | QAbstractItemView::SelectedClicked);
    m_dataTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dataTable->setAlternatingRowColors(true);
    m_dataTable->horizontalHeader()->setStretchLastSection(true);

    connect(m_dataTable, &QTableWidget::cellChanged,
            this, &MainWindow::onDataCellChanged);

    dataLayout->addLayout(buttonLayout);
    dataLayout->addWidget(m_dataTable);

    // 2. 表结构页
    auto *structurePage = new QWidget(this);
    auto *structureLayout = new QVBoxLayout(structurePage);
    structureLayout->setContentsMargins(8, 8, 8, 8);
    structureLayout->setSpacing(8);

    auto *structureButtonLayout = new QHBoxLayout;

    m_addColumnButton = new QPushButton("新增字段", structurePage);
    m_deleteColumnButton = new QPushButton("删除字段", structurePage);
    m_saveStructureButton = new QPushButton("保存结构", structurePage);

    structureButtonLayout->addWidget(m_addColumnButton);
    structureButtonLayout->addWidget(m_deleteColumnButton);
    structureButtonLayout->addWidget(m_saveStructureButton);
    structureButtonLayout->addStretch();

    m_structureTable = new QTableWidget(structurePage);

    // 允许双击编辑表结构
    m_structureTable->setEditTriggers(QAbstractItemView::DoubleClicked
                                    | QAbstractItemView::EditKeyPressed
                                    | QAbstractItemView::SelectedClicked);

    m_structureTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_structureTable->setAlternatingRowColors(true);
    m_structureTable->horizontalHeader()->setStretchLastSection(true);

    connect(m_addColumnButton, &QPushButton::clicked,
            this, &MainWindow::onAddColumn);

    connect(m_deleteColumnButton, &QPushButton::clicked,
            this, &MainWindow::onDeleteColumn);

    connect(m_saveStructureButton, &QPushButton::clicked,
            this, &MainWindow::onSaveStructure);

    connect(m_structureTable, &QTableWidget::cellChanged,
            this, &MainWindow::onStructureCellChanged);

    structureLayout->addLayout(structureButtonLayout);
    structureLayout->addWidget(m_structureTable);

    // 3. SQL 编辑器页
    auto *sqlPage = new QWidget(this);
    auto *sqlLayout = new QVBoxLayout(sqlPage);
    sqlLayout->setContentsMargins(8, 8, 8, 8);
    sqlLayout->setSpacing(8);

    auto *sqlHint = new QLabel("SQL 编辑器", sqlPage);

    m_sqlEditor = new QPlainTextEdit(sqlPage);
    m_sqlEditor->setPlaceholderText("请输入 SQL，例如：SELECT * FROM student;");
    m_sqlEditor->setPlainText("SELECT * FROM student;");

    auto *sqlButtonLayout = new QHBoxLayout;
    auto *executeButton = new QPushButton("执行 SQL", sqlPage);
    auto *clearButton = new QPushButton("清空", sqlPage);

    connect(executeButton, &QPushButton::clicked, this, &MainWindow::onExecuteSql);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::onClearSql);

    sqlButtonLayout->addWidget(executeButton);
    sqlButtonLayout->addWidget(clearButton);
    sqlButtonLayout->addStretch();

    m_sqlResultTable = new QTableWidget(sqlPage);
    m_sqlResultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sqlResultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sqlResultTable->setAlternatingRowColors(true);
    m_sqlResultTable->horizontalHeader()->setStretchLastSection(true);

    sqlLayout->addWidget(sqlHint);
    sqlLayout->addWidget(m_sqlEditor, 2);
    sqlLayout->addLayout(sqlButtonLayout);
    sqlLayout->addWidget(m_sqlResultTable, 3);

    m_tabWidget->addTab(dataPage, "表数据");
    m_tabWidget->addTab(structurePage, "表结构");
    m_tabWidget->addTab(sqlPage, "SQL 编辑器");

    rightLayout->addWidget(rightTitle);
    rightLayout->addWidget(m_tabWidget);

    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(rightPanel);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 4);

    setCentralWidget(mainSplitter);
}

void MainWindow::createStatusBar()
{
    m_statusLabel = new QLabel(this);
    statusBar()->addWidget(m_statusLabel);
}

void MainWindow::createLogDock()
{
    auto *dock = new QDockWidget("输出 / 日志", this);
    dock->setAllowedAreas(Qt::BottomDockWidgetArea);

    m_logTextEdit = new QTextEdit(dock);
    m_logTextEdit->setReadOnly(true);

    dock->setWidget(m_logTextEdit);
    addDockWidget(Qt::BottomDockWidgetArea, dock);
}

void MainWindow::initMockData()
{
    auto addTable = [this](const QString &dbName,
                           const QString &tableName,
                           const QStringList &headers,
                           const QList<QStringList> &rows,
                           const QList<ColumnMeta> &columns)
    {
        MockTable table;
        table.headers = headers;
        table.rows = rows;
        table.columns = columns;
        m_mockDatabases[dbName][tableName] = table;
    };

    addTable(
        "student_db",
        "student",
        {"id", "name", "age", "class"},
        {
            {"1", "张三", "20", "计科1班"},
            {"2", "李四", "21", "计科2班"},
            {"3", "王五", "20", "软件1班"}
        },
        {
            {"id", "INT", "NO", "PRI", "NULL", "学生编号"},
            {"name", "VARCHAR(50)", "NO", "", "NULL", "学生姓名"},
            {"age", "INT", "YES", "", "NULL", "年龄"},
            {"class", "VARCHAR(50)", "YES", "", "NULL", "班级"}
        }
    );

    addTable(
        "student_db",
        "course",
        {"id", "course_name", "teacher", "credit"},
        {
            {"1", "数据库原理", "刘老师", "3"},
            {"2", "操作系统", "陈老师", "4"},
            {"3", "计算机网络", "赵老师", "3"}
        },
        {
            {"id", "INT", "NO", "PRI", "NULL", "课程编号"},
            {"course_name", "VARCHAR(100)", "NO", "", "NULL", "课程名称"},
            {"teacher", "VARCHAR(50)", "YES", "", "NULL", "授课教师"},
            {"credit", "INT", "YES", "", "0", "学分"}
        }
    );

    addTable(
        "student_db",
        "score",
        {"id", "student_id", "course_id", "score"},
        {
            {"1", "1", "1", "92"},
            {"2", "2", "1", "88"},
            {"3", "3", "2", "95"}
        },
        {
            {"id", "INT", "NO", "PRI", "NULL", "成绩编号"},
            {"student_id", "INT", "NO", "", "NULL", "学生编号"},
            {"course_id", "INT", "NO", "", "NULL", "课程编号"},
            {"score", "INT", "YES", "", "0", "成绩"}
        }
    );

    addTable(
        "library_db",
        "book",
        {"id", "title", "author", "price"},
        {
            {"1", "数据库系统概论", "王珊", "49.00"},
            {"2", "C++ Primer", "Stanley", "89.00"},
            {"3", "算法导论", "Thomas", "108.00"}
        },
        {
            {"id", "INT", "NO", "PRI", "NULL", "图书编号"},
            {"title", "VARCHAR(100)", "NO", "", "NULL", "书名"},
            {"author", "VARCHAR(50)", "YES", "", "NULL", "作者"},
            {"price", "DOUBLE", "YES", "", "0", "价格"}
        }
    );

    addTable(
        "library_db",
        "reader",
        {"id", "name", "phone"},
        {
            {"1", "赵六", "13800000001"},
            {"2", "钱七", "13800000002"}
        },
        {
            {"id", "INT", "NO", "PRI", "NULL", "读者编号"},
            {"name", "VARCHAR(50)", "NO", "", "NULL", "读者姓名"},
            {"phone", "VARCHAR(20)", "YES", "", "NULL", "手机号"}
        }
    );

    addTable(
        "shop_db",
        "product",
        {"id", "name", "price", "stock"},
        {
            {"1", "键盘", "129.00", "50"},
            {"2", "鼠标", "59.00", "120"},
            {"3", "显示器", "799.00", "18"}
        },
        {
            {"id", "INT", "NO", "PRI", "NULL", "商品编号"},
            {"name", "VARCHAR(100)", "NO", "", "NULL", "商品名称"},
            {"price", "DOUBLE", "YES", "", "0", "价格"},
            {"stock", "INT", "YES", "", "0", "库存"}
        }
    );
}

void MainWindow::loadExplorerFromServer()
{
    if (!m_dbClient->isConnected()) {
        m_databaseTree->clear();

        auto *placeholderItem = new QTreeWidgetItem(m_databaseTree);
        placeholderItem->setText(0, "未连接服务端");
        placeholderItem->setData(0, RoleType, "placeholder");

        return;
    }

    m_databaseTree->clear();
    m_databaseItems.clear();

    m_requestMode = ClientRequestMode::LoadDatabases;

    m_statusLabel->setText("正在从服务端加载数据库列表...");
    appendLog("请求服务端数据库列表：SHOW DATABASES;");

    m_dbClient->executeSql("SHOW DATABASES;");
}

void MainWindow::requestTablesForDatabase(const QString& databaseName)
{
    if (!m_dbClient->isConnected()) {
        QMessageBox::warning(this, "未连接", "请先连接服务端。");
        return;
    }

    m_pendingDatabase = databaseName;
    m_pendingTable.clear();

    m_requestMode = ClientRequestMode::LoadTablesUseDatabase;

    m_statusLabel->setText(QString("正在选择数据库：%1").arg(databaseName));
    appendLog(QString("发送 SQL：USE DATABASE %1;").arg(databaseName));

    m_dbClient->executeSql(QString("USE DATABASE %1;").arg(databaseName));
}

void MainWindow::openTableFromServer(const QString& databaseName, const QString& tableName)
{
    if (!m_dbClient->isConnected()) {
        QMessageBox::warning(this, "未连接", "请先连接服务端。");
        return;
    }

    m_pendingDatabase = databaseName;
    m_pendingTable = tableName;

    m_requestMode = ClientRequestMode::OpenTableUseDatabase;

    m_statusLabel->setText(QString("正在打开表：%1.%2").arg(databaseName, tableName));
    appendLog(QString("发送 SQL：USE DATABASE %1;").arg(databaseName));

    m_dbClient->executeSql(QString("USE DATABASE %1;").arg(databaseName));
}

void MainWindow::fillDataTable(const QStringList& headers, const QVector<QStringList>& rows)
{
    m_isLoadingTable = true;

    m_originalRows.clear();

    m_dataTable->clear();
    m_dataTable->setColumnCount(headers.size());
    m_dataTable->setRowCount(rows.size());
    m_dataTable->setHorizontalHeaderLabels(headers);

    for (int row = 0; row < rows.size(); ++row) {
        const QStringList& rowData = rows[row];
        m_originalRows.append(rowData);

        for (int col = 0; col < headers.size(); ++col) {
            const QString value = col < rowData.size() ? rowData[col] : "";
            m_dataTable->setItem(row, col, new QTableWidgetItem(value));
        }
    }

    m_dataTable->resizeColumnsToContents();
    m_dataTable->horizontalHeader()->setStretchLastSection(true);

    m_isLoadingTable = false;
    m_hasUnsavedChanges = false;
}

void MainWindow::loadExplorer()
{
    m_databaseTree->clear();

    auto *connectionItem = new QTreeWidgetItem(m_databaseTree);
    connectionItem->setText(0, m_connectionName);
    connectionItem->setData(0, RoleType, "connection");

    for (auto dbIt = m_mockDatabases.constBegin(); dbIt != m_mockDatabases.constEnd(); ++dbIt) {
        const QString &dbName = dbIt.key();

        auto *dbItem = new QTreeWidgetItem(connectionItem);
        dbItem->setText(0, dbName);
        dbItem->setData(0, RoleType, "database");
        dbItem->setData(0, RoleDatabase, dbName);

        auto *tablesFolder = new QTreeWidgetItem(dbItem);
        tablesFolder->setText(0, "Tables");
        tablesFolder->setData(0, RoleType, "folder");

        const auto &tables = dbIt.value();
        for (auto tableIt = tables.constBegin(); tableIt != tables.constEnd(); ++tableIt) {
            const QString &tableName = tableIt.key();

            auto *tableItem = new QTreeWidgetItem(tablesFolder);
            tableItem->setText(0, tableName);
            tableItem->setData(0, RoleType, "table");
            tableItem->setData(0, RoleDatabase, dbName);
            tableItem->setData(0, RoleTable, tableName);
        }
    }

    m_databaseTree->expandAll();
}

void MainWindow::openTable(const QString &databaseName, const QString &tableName)
{
    if (!m_mockDatabases.contains(databaseName) ||
        !m_mockDatabases[databaseName].contains(tableName)) {
        return;
    }

    m_currentDatabase = databaseName;
    m_currentTable = tableName;
    m_hasUnsavedChanges = false;

    const MockTable table = m_mockDatabases[databaseName][tableName];
    fillDataTable(table);
    fillStructureTable(table);

    fillSqlResultTable(tableName);

    m_tabWidget->setCurrentIndex(0);

    updateWindowCaption();
    m_statusLabel->setText(QString("已打开表：%1.%2").arg(databaseName, tableName));
    appendLog(QString("打开表：%1.%2").arg(databaseName, tableName));
}

void MainWindow::fillDataTable(const MockTable &table)
{
    m_isLoadingTable = true;

    m_originalRows.clear();
    for (const QStringList &rowData : table.rows) {
        m_originalRows.append(rowData);
    }
    m_dataTable->clear();
    m_dataTable->setColumnCount(table.headers.size());
    m_dataTable->setRowCount(table.rows.size());
    m_dataTable->setHorizontalHeaderLabels(table.headers);

    for (int row = 0; row < table.rows.size(); ++row) {
        const QStringList &rowData = table.rows[row];
        for (int col = 0; col < rowData.size(); ++col) {
            auto *item = new QTableWidgetItem(rowData[col]);
            m_dataTable->setItem(row, col, item);
        }
    }

    m_dataTable->resizeColumnsToContents();
    m_dataTable->horizontalHeader()->setStretchLastSection(true);

    m_isLoadingTable = false;
    QVector<QStringList> rows;
    for (const QStringList& row : table.rows) {
        rows.append(row);
    }
    setOriginalDataSnapshot(table.headers, rows);
}

void MainWindow::fillStructureTable(const MockTable &table)
{
    m_isLoadingTable = true;

    m_structureTable->clear();
    m_structureTable->setColumnCount(6);
    m_structureTable->setRowCount(table.columns.size());
    m_structureTable->setHorizontalHeaderLabels(
        {"字段名", "类型", "是否为空", "主键", "默认值", "说明"}
    );

    for (int row = 0; row < table.columns.size(); ++row) {
        const ColumnMeta &col = table.columns[row];

        m_structureTable->setItem(row, 0, new QTableWidgetItem(col.name));
        m_structureTable->setItem(row, 1, new QTableWidgetItem(col.type));
        m_structureTable->setItem(row, 2, new QTableWidgetItem(col.nullable));
        m_structureTable->setItem(row, 3, new QTableWidgetItem(col.key));
        m_structureTable->setItem(row, 4, new QTableWidgetItem(col.defaultValue));
        m_structureTable->setItem(row, 5, new QTableWidgetItem(col.comment));
    }

    m_structureTable->resizeColumnsToContents();
    m_structureTable->horizontalHeader()->setStretchLastSection(true);

    m_isLoadingTable = false;
}

void MainWindow::fillSqlResultTable(const QString &tableName)
{
    if (m_currentDatabase.isEmpty() || m_currentTable.isEmpty()) {
        m_sqlResultTable->clear();
        m_sqlResultTable->setRowCount(0);
        m_sqlResultTable->setColumnCount(0);
        return;
    }

    const MockTable table = m_mockDatabases[m_currentDatabase][tableName];

    m_sqlResultTable->clear();
    m_sqlResultTable->setColumnCount(table.headers.size());
    m_sqlResultTable->setRowCount(table.rows.size());
    m_sqlResultTable->setHorizontalHeaderLabels(table.headers);

    for (int row = 0; row < table.rows.size(); ++row) {
        const QStringList &rowData = table.rows[row];
        for (int col = 0; col < rowData.size(); ++col) {
            m_sqlResultTable->setItem(row, col, new QTableWidgetItem(rowData[col]));
        }
    }

    m_sqlResultTable->resizeColumnsToContents();
    m_sqlResultTable->horizontalHeader()->setStretchLastSection(true);
}

void MainWindow::fillSqlResultTable(const QStringList& headers, const QVector<QStringList>& rows)
{
    m_sqlResultTable->clear();
    m_sqlResultTable->setColumnCount(headers.size());
    m_sqlResultTable->setRowCount(rows.size());
    m_sqlResultTable->setHorizontalHeaderLabels(headers);

    for (int row = 0; row < rows.size(); ++row) {
        const QStringList &rowData = rows[row];
        for (int col = 0; col < headers.size(); ++col) {
            const QString value = col < rowData.size() ? rowData[col] : "";
            m_sqlResultTable->setItem(row, col, new QTableWidgetItem(value));
        }
    }

    m_sqlResultTable->resizeColumnsToContents();
    m_sqlResultTable->horizontalHeader()->setStretchLastSection(true);
}

QStringList MainWindow::currentDataHeaders() const
{
    QStringList headers;
    for (int col = 0; col < m_dataTable->columnCount(); ++col) {
        QTableWidgetItem *item = m_dataTable->horizontalHeaderItem(col);
        headers << (item ? item->text() : QString("col%1").arg(col + 1));
    }
    return headers;
}

QStringList MainWindow::currentDataRow(int row) const
{
    QStringList rowData;
    for (int col = 0; col < m_dataTable->columnCount(); ++col) {
        QTableWidgetItem *item = m_dataTable->item(row, col);
        rowData << (item ? item->text() : "");
    }
    return rowData;
}

QString MainWindow::sqlLiteral(const QString& value, bool* ok) const
{
    if (ok && !*ok) return QString();
    if (value.compare("NULL", Qt::CaseInsensitive) == 0) {
        return "NULL";
    }

    if (value.contains('\'')) {
        if (ok) *ok = false;
        return QString();
    }

    if (ok) *ok = true;
    QString escaped = value;
    return QString("'%1'").arg(escaped);
}

QString MainWindow::extractSimpleSelectTable(const QString& sql) const
{
    QRegularExpression re(
        R"(^\s*select\s+\*\s+from\s+([A-Za-z_][A-Za-z0-9_]*)\s*;?\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(sql);
    return match.hasMatch() ? match.captured(1) : QString();
}

QString MainWindow::buildInsertSql(const QStringList& headers, const QStringList& row, bool* ok) const
{
    QStringList values;
    for (int i = 0; i < headers.size(); ++i) {
        values << sqlLiteral(i < row.size() ? row[i] : "", ok);
    }
    return QString("INSERT INTO %1 (%2) VALUES (%3);")
        .arg(m_currentTable, headers.join(", "), values.join(", "));
}

QString MainWindow::buildDeleteSql(const QString& keyColumn, const QString& keyValue, bool* ok) const
{
    return QString("DELETE FROM %1 WHERE %2 = %3;")
        .arg(m_currentTable, keyColumn, sqlLiteral(keyValue, ok));
}

QString MainWindow::buildUpdateSql(const QStringList& headers,
                                   const QStringList& oldRow,
                                   const QStringList& newRow,
                                   bool* ok) const
{
    QStringList assignments;
    for (int i = 1; i < headers.size(); ++i) {
        const QString oldValue = i < oldRow.size() ? oldRow[i] : "";
        const QString newValue = i < newRow.size() ? newRow[i] : "";
        if (oldValue != newValue) {
            assignments << QString("%1 = %2").arg(headers[i], sqlLiteral(newValue, ok));
        }
    }

    if (assignments.isEmpty()) {
        return QString();
    }

    return QString("UPDATE %1 SET %2 WHERE %3 = %4;")
        .arg(m_currentTable,
             assignments.join(", "),
             headers[0],
             sqlLiteral(oldRow.value(0), ok));
}

void MainWindow::setOriginalDataSnapshot(const QStringList& headers, const QVector<QStringList>& rows)
{
    m_originalDataHeaders = headers;
    m_originalDataRows = rows;
}

void MainWindow::startNextSaveSql()
{
    if (m_pendingSaveSql.isEmpty()) {
        finishTableSave();
        return;
    }

    const QString sql = m_pendingSaveSql.takeFirst();
    appendLog(QString("保存发出 SQL：%1").arg(sql));
    m_lastSubmittedSql = sql;
    m_dbClient->executeSql(sql);
}

void MainWindow::finishTableSave()
{
    m_isSavingTableChanges = false;
    m_hasUnsavedChanges = false;
    m_statusLabel->setText("表数据已保存，正在刷新...");

    if (!m_pendingRefreshSql.isEmpty()) {
        m_isRefreshingAfterSave = true;
        m_lastSubmittedSql = m_pendingRefreshSql;
        m_dbClient->executeSql(m_pendingRefreshSql);
    } else {
        m_isRefreshingAfterSave = false;
        QMessageBox::information(this, "保存成功", "表数据已保存到服务端。");
    }
}

void MainWindow::appendLog(const QString &message)
{
    m_logTextEdit->append(message);
}

void MainWindow::updateWindowCaption()
{
    QString title = "RuanKoDBMS Client";

    if (!m_currentDatabase.isEmpty() && !m_currentTable.isEmpty()) {
        title += QString(" - %1.%2").arg(m_currentDatabase, m_currentTable);
    } else {
        title += " - 图形化客户端原型";
    }

    setWindowTitle(title);
}

void MainWindow::onNewConnection()
{
    ConnectDialog dialog(this);

    if (dialog.exec() == QDialog::Accepted) {
        m_connectionName = dialog.connectionName().isEmpty()
            ? "本地连接"
            : dialog.connectionName();

        updateWindowCaption();

        QString msg = QString("正在连接：%1@%2:%3")
                          .arg(dialog.username())
                          .arg(dialog.host())
                          .arg(dialog.port());

        m_statusLabel->setText(msg);
        appendLog(msg);
        m_loginPending = true;
        m_dbClient->connectToServer(
            dialog.host(),
            static_cast<quint16>(dialog.port()),
            dialog.username(),
            dialog.password());
    }
}

void MainWindow::onRefresh()
{
    if (!m_dbClient->isConnected()) {
        QMessageBox::information(this, "未连接", "请先连接服务端。");
        return;
    }

    if (!m_currentDatabase.isEmpty() && !m_currentTable.isEmpty()) {
        openTableFromServer(m_currentDatabase, m_currentTable);
        appendLog("刷新当前表。");
        return;
    }

    loadExplorerFromServer();
}


void MainWindow::onTreeItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);

    const QString type = item->data(0, RoleType).toString();

    if (type == "database") {
        m_currentDatabase = item->data(0, RoleDatabase).toString();
        m_currentTable.clear();

        m_statusLabel->setText(QString("已选择数据库：%1").arg(m_currentDatabase));
        appendLog(QString("选择数据库：%1").arg(m_currentDatabase));
        updateWindowCaption();

        requestTablesForDatabase(m_currentDatabase);
        return;
    }

    if (type == "table") {
        const QString databaseName = item->data(0, RoleDatabase).toString();
        const QString tableName = item->data(0, RoleTable).toString();

        openTableFromServer(databaseName, tableName);
        return;
    }
}


void MainWindow::onExecuteSql()
{
    const QString sql = m_sqlEditor->toPlainText().trimmed();

    if (sql.isEmpty()) {
        QMessageBox::warning(this, "提示", "SQL 不能为空。");
        return;
    }

    if (!m_dbClient->isConnected()) {
        QMessageBox::warning(this, "未连接", "请先通过“新建连接”连接服务端。");
        return;
    }

    m_tabWidget->setCurrentIndex(2);
    m_statusLabel->setText("正在执行 SQL...");
    appendLog(QString("发送 SQL：%1").arg(sql));
    m_lastSubmittedSql = sql;
    m_dbClient->executeSql(sql);
}

void MainWindow::onClearSql()
{
    m_sqlEditor->clear();
    m_sqlResultTable->clear();
    m_sqlResultTable->setRowCount(0);
    m_sqlResultTable->setColumnCount(0);

    m_statusLabel->setText("SQL 编辑器已清空。");
    appendLog("清空 SQL 编辑器。");
}

void MainWindow::onAddRow()
{
    if (m_currentTable.isEmpty()) {
        QMessageBox::information(this, "提示", "请先从左侧选择一张表。");
        return;
    }

    const int row = m_dataTable->rowCount();
    m_dataTable->insertRow(row);

    for (int col = 0; col < m_dataTable->columnCount(); ++col) {
        m_dataTable->setItem(row, col, new QTableWidgetItem(""));
    }

    m_hasUnsavedChanges = true;
    m_statusLabel->setText("已新增一行，请记得保存修改。");
    appendLog("新增一行。");
}

void MainWindow::onDeleteRow()
{
    if (m_currentTable.isEmpty()) {
        QMessageBox::information(this, "提示", "请先从左侧选择一张表。");
        return;
    }

    const auto selectedItems = m_dataTable->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选中要删除的行。");
        return;
    }

    QSet<int> rowsToDelete;
    for (auto *item : selectedItems) {
        rowsToDelete.insert(item->row());
    }

    QList<int> rows = rowsToDelete.values();
    std::sort(rows.begin(), rows.end(), std::greater<int>());

    for (int row : rows) {
        m_dataTable->removeRow(row);
    }

    m_hasUnsavedChanges = true;
    m_statusLabel->setText("已删除选中行，请记得保存修改。");
    appendLog("删除选中行。");
}

void MainWindow::onSaveChanges()
{
    if (m_currentDatabase.isEmpty() || m_currentTable.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择一张表。");
        return;
    }

    if (!m_dbClient->isConnected()) {
        QMessageBox::warning(this, "未连接", "请先通过“新建连接”连接服务端。");
        return;
    }

    if (!isSafeIdentifier(m_currentDatabase) || !isSafeIdentifier(m_currentTable)) {
        QMessageBox::warning(
            this,
            "保存失败",
            "数据库名和表名必须以字母或下划线开头，只能包含字母、数字、下划线。\n"
            "例如：library_db、book、student_db。"
        );
        return;
    }

    if (m_dataTable->columnCount() == 0) {
        QMessageBox::warning(this, "保存失败", "当前表没有字段。");
        return;
    }

    QStringList columns;

    for (int col = 0; col < m_dataTable->columnCount(); ++col) {
        QTableWidgetItem *headerItem = m_dataTable->horizontalHeaderItem(col);
        const QString columnName = headerItem ? headerItem->text().trimmed() : "";

        if (!isSafeIdentifier(columnName)) {
            QMessageBox::warning(
                this,
                "保存失败",
                QString("字段名不合法：%1").arg(columnName)
            );
            return;
        }

        columns << columnName;
    }

    const QString primaryKeyColumn = columns.first();

    QStringList statements;

    // 关键：先切换到左侧当前表所在数据库。
    statements << QString("USE DATABASE %1").arg(m_currentDatabase);

    // 简化版保存策略：
    // 先删除打开表时已有的旧行，再插入当前界面里的所有行。
    // 默认第一列是主键，例如 id。
    for (const QStringList &oldRow : m_originalRows) {
    if (oldRow.isEmpty()) {
        continue;
    }

    bool ok = true;
    const QString pkValue = sqlLiteral(oldRow.first(), &ok);

    if (!ok) {
        QMessageBox::warning(this, "保存失败", QString("主键值不合法：%1").arg(oldRow.first()));
        return;
    }

    statements << QString("DELETE FROM %1 WHERE %2 = %3")
                      .arg(m_currentTable)
                      .arg(primaryKeyColumn)
                      .arg(pkValue);
}


    for (int row = 0; row < m_dataTable->rowCount(); ++row) {
    QStringList values;

    for (int col = 0; col < m_dataTable->columnCount(); ++col) {
        bool ok = true;
        const QString cellValue = tableCellText(m_dataTable, row, col);
        const QString literalValue = sqlLiteral(cellValue, &ok);

        if (!ok) {
            QMessageBox::warning(
                this,
                "保存失败",
                QString("第 %1 行第 %2 列的值不合法：%3")
                    .arg(row + 1)
                    .arg(col + 1)
                    .arg(cellValue)
            );
            return;
        }

        values << literalValue;
    }

    statements << QString("INSERT INTO %1 (%2) VALUES (%3)")
                      .arg(m_currentTable)
                      .arg(columns.join(", "))
                      .arg(values.join(", "));
}

    enqueueServerSql(statements);
}

void MainWindow::enqueueServerSql(const QStringList &statements)
{
    m_pendingSqlQueue.clear();

    for (const QString &statement : statements) {
        const QString trimmed = statement.trimmed();
        if (!trimmed.isEmpty()) {
            m_pendingSqlQueue.enqueue(trimmed);
        }
    }

    if (m_pendingSqlQueue.isEmpty()) {
        return;
    }

    m_isSavingToServer = true;
    m_statusLabel->setText("正在保存到服务端...");
    executeNextQueuedSql();
}

void MainWindow::executeNextQueuedSql()
{
    if (m_pendingSqlQueue.isEmpty()) {
        m_isSavingToServer = false;
        m_hasUnsavedChanges = false;

        m_originalRows.clear();

        for (int row = 0; row < m_dataTable->rowCount(); ++row) {
            QStringList rowData;

            for (int col = 0; col < m_dataTable->columnCount(); ++col) {
                rowData << tableCellText(m_dataTable, row, col);
            }

            m_originalRows.append(rowData);
        }

        m_statusLabel->setText("表数据已保存到服务端。");
        QMessageBox::information(this, "保存成功", "表数据已写入服务端。");
        return;
    }

    const QString sql = m_pendingSqlQueue.dequeue();

    appendLog(QString("保存发出 SQL：%1;").arg(sql));
    m_dbClient->executeSql(sql + ";");
}


void MainWindow::onDataCellChanged(int row, int column)
{
    Q_UNUSED(row);
    Q_UNUSED(column);

    if (m_isLoadingTable) {
        return;
    }

    m_hasUnsavedChanges = true;
    m_statusLabel->setText("表数据已修改，尚未保存。");
}

void MainWindow::onAbout()
{
    QMessageBox::information(
        this,
        "关于",
        "RuanKoDBMS Client\n\n"
        "这是一个类似 MySQL / Navicat 风格的图形化客户端原型。\n"
        "当前版本使用模拟数据，可进行界面展示与交互演示。\n"
        "后续可接入 client/network 模块实现真实通信。"
    );
}

void MainWindow::onAddColumn()
{
    if (m_currentDatabase.isEmpty() || m_currentTable.isEmpty()) {
        QMessageBox::information(this, "提示", "请先从左侧选择一张表。");
        return;
    }
    if (m_structureTable->columnCount() == 0) {
    m_structureTable->setColumnCount(6);
    m_structureTable->setHorizontalHeaderLabels(
        {"字段名", "类型", "是否为空", "主键", "默认值", "说明"}
    );
    }

    int row = m_structureTable->rowCount();
    m_structureTable->insertRow(row);

    m_structureTable->setItem(row, 0, new QTableWidgetItem("new_column"));
    m_structureTable->setItem(row, 1, new QTableWidgetItem("VARCHAR(50)"));
    m_structureTable->setItem(row, 2, new QTableWidgetItem("YES"));
    m_structureTable->setItem(row, 3, new QTableWidgetItem(""));
    m_structureTable->setItem(row, 4, new QTableWidgetItem("NULL"));
    m_structureTable->setItem(row, 5, new QTableWidgetItem(""));

    m_hasUnsavedChanges = true;
    m_statusLabel->setText("已新增字段，请保存表结构。");
    appendLog("新增字段。");
}

void MainWindow::onDeleteColumn()
{
    if (m_currentDatabase.isEmpty() || m_currentTable.isEmpty()) {
        QMessageBox::information(this, "提示", "请先从左侧选择一张表。");
        return;
    }

    const auto selectedItems = m_structureTable->selectedItems();

    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选中要删除的字段。");
        return;
    }

    QSet<int> rowsToDelete;

    for (auto *item : selectedItems) {
        rowsToDelete.insert(item->row());
    }

    QList<int> rows = rowsToDelete.values();
    std::sort(rows.begin(), rows.end(), std::greater<int>());

    for (int row : rows) {
        m_structureTable->removeRow(row);
    }

    m_hasUnsavedChanges = true;
    m_statusLabel->setText("已删除字段，请保存表结构。");
    appendLog("删除字段。");
}

void MainWindow::onSaveStructure()
{
    if (m_currentDatabase.isEmpty() || m_currentTable.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择一张表。");
        return;
    }

    if (!m_dbClient->isConnected()) {
        QMessageBox::warning(this, "未连接", "请先通过“新建连接”连接服务端。");
        return;
    }

    if (!isSafeIdentifier(m_currentDatabase) || !isSafeIdentifier(m_currentTable)) {
        QMessageBox::warning(this, "保存失败", "数据库名或表名不合法。");
        return;
    }

    const QList<ColumnMeta> newColumns = currentStructureColumns();

    if (newColumns.isEmpty()) {
        QMessageBox::warning(this, "保存失败", "表结构不能为空。");
        return;
    }

    QStringList alterSql = buildStructureAlterSql(m_originalColumns, newColumns);

    if (alterSql.isEmpty()) {
        QMessageBox::information(this, "提示", "表结构没有需要保存的修改。");
        return;
    }

    m_pendingStructureSql.clear();
    m_pendingStructureSql << QString("USE DATABASE %1;").arg(m_currentDatabase);
    m_pendingStructureSql << alterSql;

    m_isSavingStructure = true;
    m_statusLabel->setText("正在保存表结构到服务端...");

    startNextStructureSql();
}


void MainWindow::onStructureCellChanged(int row, int column)
{
    Q_UNUSED(row);
    Q_UNUSED(column);

    if (m_isLoadingTable) {
        return;
    }

    m_hasUnsavedChanges = true;
    m_statusLabel->setText("表结构已修改，尚未保存。");
}

void MainWindow::onCreateDatabase()
{
    if (!m_dbClient->isConnected()) {
        QMessageBox::warning(this, "未连接", "请先通过“新建连接”连接服务端。");
        return;
    }

    bool ok = false;

    QString databaseName = QInputDialog::getText(
        this,
        "新建数据库",
        "请输入数据库名称：",
        QLineEdit::Normal,
        "",
        &ok
    );

    if (!ok) {
        return;
    }

    databaseName = databaseName.trimmed();

    if (databaseName.isEmpty()) {
        QMessageBox::warning(this, "创建失败", "数据库名称不能为空。");
        return;
    }

    if (!isSafeIdentifier(databaseName)) {
        QMessageBox::warning(
            this,
            "创建失败",
            "数据库名必须以字母或下划线开头，只能包含字母、数字、下划线。\n"
            "例如：testdb、library_db、student_db。"
        );
        return;
    }

    m_pendingDatabase = databaseName;
    m_pendingTable.clear();
    m_requestMode = ClientRequestMode::CreateDatabase;

    const QString sql = QString("CREATE DATABASE %1;").arg(databaseName);

    m_statusLabel->setText(QString("正在创建数据库：%1").arg(databaseName));
    appendLog(QString("发送 SQL：%1").arg(sql));

    m_dbClient->executeSql(sql);
}

void MainWindow::onDeleteDatabase()
{
    if (!m_dbClient->isConnected()) {
        QMessageBox::warning(this, "未连接", "请先通过“新建连接”连接服务端。");
        return;
    }

    QString databaseName = m_currentDatabase.trimmed();

    if (databaseName.isEmpty()) {
        QMessageBox::information(this, "提示", "请先在左侧选择要删除的数据库。");
        return;
    }

    if (!isSafeIdentifier(databaseName)) {
        QMessageBox::warning(
            this,
            "删除失败",
            "数据库名不合法，不能发送到服务端。"
        );
        return;
    }

    int ret = QMessageBox::question(
        this,
        "确认删除",
        QString("确定要删除数据库 %1 吗？\n该操作会发送到服务端，删除后不可恢复。").arg(databaseName),
        QMessageBox::Yes | QMessageBox::No
    );

    if (ret != QMessageBox::Yes) {
        return;
    }

    m_pendingDatabase = databaseName;
    m_pendingTable.clear();
    m_requestMode = ClientRequestMode::DeleteDatabase;

    const QString sql = QString("DROP DATABASE %1;").arg(databaseName);

    m_statusLabel->setText(QString("正在删除数据库：%1").arg(databaseName));
    appendLog(QString("发送 SQL：%1").arg(sql));

    m_dbClient->executeSql(sql);
}

void MainWindow::onCreateTable()
{
    if (!m_dbClient->isConnected()) {
        QMessageBox::warning(this, "未连接", "请先通过“新建连接”连接服务端。");
        return;
    }

    if (m_currentDatabase.isEmpty()) {
        QMessageBox::information(this, "提示", "请先在左侧选择一个数据库。");
        return;
    }

    if (!isSafeIdentifier(m_currentDatabase)) {
        QMessageBox::warning(this, "创建失败", "当前数据库名不合法。");
        return;
    }

    bool ok = false;

    QString tableName = QInputDialog::getText(
        this,
        "新建表",
        QString("当前数据库：%1\n请输入表名：").arg(m_currentDatabase),
        QLineEdit::Normal,
        "",
        &ok
    );

    if (!ok) {
        return;
    }

    tableName = tableName.trimmed();

    if (tableName.isEmpty()) {
        QMessageBox::warning(this, "创建失败", "表名不能为空。");
        return;
    }

    if (!isSafeIdentifier(tableName)) {
        QMessageBox::warning(
            this,
            "创建失败",
            "表名必须以字母或下划线开头，只能包含字母、数字、下划线。\n"
            "例如：student、book、table45。"
        );
        return;
    }

    m_pendingDatabase = m_currentDatabase;
    m_pendingTable = tableName;
    m_requestMode = ClientRequestMode::CreateTableUseDatabase;

    const QString sql = QString("USE DATABASE %1;").arg(m_currentDatabase);

    m_statusLabel->setText(QString("正在选择数据库：%1").arg(m_currentDatabase));
    appendLog(QString("发送 SQL：%1").arg(sql));

    m_dbClient->executeSql(sql);
}

void MainWindow::onDeleteTable()
{
    if (!m_dbClient->isConnected()) {
        QMessageBox::warning(this, "未连接", "请先通过“新建连接”连接服务端。");
        return;
    }

    if (m_currentDatabase.isEmpty() || m_currentTable.isEmpty()) {
        QMessageBox::information(this, "提示", "请先在左侧选择一张表。");
        return;
    }

    if (!isSafeIdentifier(m_currentDatabase) || !isSafeIdentifier(m_currentTable)) {
        QMessageBox::warning(this, "删除失败", "数据库名或表名不合法。");
        return;
    }

    int ret = QMessageBox::question(
        this,
        "确认删除",
        QString("确定要删除表 %1.%2 吗？\n该操作会发送到服务端，删除后不可恢复。")
            .arg(m_currentDatabase, m_currentTable),
        QMessageBox::Yes | QMessageBox::No
    );

    if (ret != QMessageBox::Yes) {
        return;
    }

    m_pendingDatabase = m_currentDatabase;
    m_pendingTable = m_currentTable;
    m_requestMode = ClientRequestMode::DeleteTableUseDatabase;

    m_statusLabel->setText(QString("正在删除表：%1.%2")
                               .arg(m_currentDatabase, m_currentTable));

    appendLog(QString("发送 SQL：USE DATABASE %1;").arg(m_currentDatabase));

    m_dbClient->executeSql(QString("USE DATABASE %1;").arg(m_currentDatabase));
}


void MainWindow::onServerConnected()
{
    m_statusLabel->setText("已连接服务端，正在登录...");
    appendLog("已建立网络连接。");
}

void MainWindow::onServerLoginSucceeded()
{
    m_loginPending = false;
    const QString msg = QString("连接成功：%1").arg(m_connectionName);
    m_statusLabel->setText(msg);
    appendLog(QString("登录成功：%1").arg(m_dbClient->username()));
    QMessageBox::information(this, "连接成功", "已连接到服务端。");
    loadExplorerFromServer();
}

void MainWindow::onServerResponse(const DbClient::QueryResponse& response)
{
        if (m_requestMode != ClientRequestMode::None) {
        if (!response.success) {
            QMessageBox::warning(this, "服务端返回错误", response.message);
            appendLog(response.message);
            m_requestMode = ClientRequestMode::None;
            return;
        }

        if (m_requestMode == ClientRequestMode::CreateDatabase) {
            appendLog(response.message);

            const QString createdDatabase = m_pendingDatabase;

            m_requestMode = ClientRequestMode::None;
            m_pendingDatabase.clear();
            m_pendingTable.clear();

            m_statusLabel->setText(QString("数据库 %1 创建成功。").arg(createdDatabase));

            QMessageBox::information(
                this,
                "创建成功",
                QString("数据库 %1 已创建到服务端。").arg(createdDatabase)
            );

            loadExplorerFromServer();
            return;
        }

        if (m_requestMode == ClientRequestMode::DeleteDatabase) {
            appendLog(response.message);

            const QString deletedDatabase = m_pendingDatabase;

            m_requestMode = ClientRequestMode::None;
            m_pendingDatabase.clear();
            m_pendingTable.clear();

            if (m_currentDatabase == deletedDatabase) {
                m_currentDatabase.clear();
                m_currentTable.clear();

                m_dataTable->clear();
                m_dataTable->setRowCount(0);
                m_dataTable->setColumnCount(0);

                m_structureTable->clear();
                m_structureTable->setRowCount(0);
                m_structureTable->setColumnCount(0);

                m_sqlResultTable->clear();
                m_sqlResultTable->setRowCount(0);
                m_sqlResultTable->setColumnCount(0);

                updateWindowCaption();
            }

            m_statusLabel->setText(QString("数据库 %1 已从服务端删除。").arg(deletedDatabase));

            QMessageBox::information(
                this,
                "删除成功",
                QString("数据库 %1 已从服务端删除。").arg(deletedDatabase)
            );

            loadExplorerFromServer();
            return;
        }
if (m_requestMode == ClientRequestMode::DeleteTableUseDatabase) {
    m_requestMode = ClientRequestMode::DeleteTableDrop;

    const QString sql = QString("DROP TABLE %1;").arg(m_pendingTable);

    appendLog(QString("发送 SQL：%1").arg(sql));
    m_dbClient->executeSql(sql);
    return;
}

if (m_requestMode == ClientRequestMode::DeleteTableDrop) {
    appendLog(response.message);

    const QString deletedDatabase = m_pendingDatabase;
    const QString deletedTable = m_pendingTable;

    m_requestMode = ClientRequestMode::None;
    m_pendingDatabase.clear();
    m_pendingTable.clear();

    if (m_currentDatabase == deletedDatabase && m_currentTable == deletedTable) {
        m_currentTable.clear();

        m_dataTable->clear();
        m_dataTable->setRowCount(0);
        m_dataTable->setColumnCount(0);

        m_structureTable->clear();
        m_structureTable->setRowCount(0);
        m_structureTable->setColumnCount(0);

        m_sqlResultTable->clear();
        m_sqlResultTable->setRowCount(0);
        m_sqlResultTable->setColumnCount(0);

        updateWindowCaption();
    }

    m_statusLabel->setText(QString("表 %1.%2 已从服务端删除。")
                               .arg(deletedDatabase, deletedTable));

    QMessageBox::information(
        this,
        "删除成功",
        QString("表 %1.%2 已从服务端删除。").arg(deletedDatabase, deletedTable)
    );

    m_currentDatabase = deletedDatabase;
    m_currentTable.clear();

    requestTablesForDatabase(deletedDatabase);
    return;
}
        if (m_requestMode == ClientRequestMode::CreateTableUseDatabase) {
    m_requestMode = ClientRequestMode::CreateTableCreate;

    const QString sql = QString(
        "CREATE TABLE %1 (id INT PRIMARY KEY, name VARCHAR(50));"
    ).arg(m_pendingTable);

    m_statusLabel->setText(QString("正在创建表：%1.%2")
                               .arg(m_pendingDatabase, m_pendingTable));

    appendLog(QString("发送 SQL：%1").arg(sql));

    m_dbClient->executeSql(sql);
    return;
}

if (m_requestMode == ClientRequestMode::CreateTableCreate) {
    appendLog(response.message);

    const QString createdDatabase = m_pendingDatabase;
    const QString createdTable = m_pendingTable;

    m_requestMode = ClientRequestMode::None;
    m_pendingDatabase.clear();
    m_pendingTable.clear();

    m_currentDatabase = createdDatabase;
    m_currentTable = createdTable;

    m_statusLabel->setText(QString("表 %1.%2 已创建到服务端。")
                               .arg(createdDatabase, createdTable));

    QMessageBox::information(
        this,
        "创建成功",
        QString("表 %1.%2 已创建到服务端。").arg(createdDatabase, createdTable)
    );

    requestTablesForDatabase(createdDatabase);
    return;
}


        if (m_requestMode == ClientRequestMode::LoadDatabases) {
            m_databaseTree->clear();
            m_databaseItems.clear();

            auto *connectionItem = new QTreeWidgetItem(m_databaseTree);
            connectionItem->setText(0, m_connectionName.isEmpty() ? "服务端连接" : m_connectionName);
            connectionItem->setData(0, RoleType, "connection");

            for (const QStringList& row : response.rows) {
                if (row.isEmpty()) {
                    continue;
                }

                const QString databaseName = row[0].trimmed();

                if (databaseName.isEmpty()) {
                    continue;
                }

                auto *dbItem = new QTreeWidgetItem(connectionItem);
                dbItem->setText(0, databaseName);
                dbItem->setData(0, RoleType, "database");
                dbItem->setData(0, RoleDatabase, databaseName);

                auto *tablesFolder = new QTreeWidgetItem(dbItem);
                tablesFolder->setText(0, "Tables");
                tablesFolder->setData(0, RoleType, "folder");

                auto *hintItem = new QTreeWidgetItem(tablesFolder);
                hintItem->setText(0, "点击数据库加载表");
                hintItem->setData(0, RoleType, "placeholder");

                m_databaseItems[databaseName] = dbItem;
            }

            m_databaseTree->expandItem(connectionItem);

            m_statusLabel->setText("数据库列表已从服务端加载。");
            appendLog("数据库列表已从服务端加载。");

            m_requestMode = ClientRequestMode::None;
            return;
        }

        if (m_requestMode == ClientRequestMode::LoadTablesUseDatabase) {
            m_requestMode = ClientRequestMode::LoadTables;

            appendLog("发送 SQL：SHOW TABLES;");
            m_dbClient->executeSql("SHOW TABLES;");
            return;
        }

        if (m_requestMode == ClientRequestMode::LoadTables) {
            QTreeWidgetItem *dbItem = m_databaseItems.value(m_pendingDatabase, nullptr);

            if (!dbItem) {
                m_requestMode = ClientRequestMode::None;
                return;
            }

            qDeleteAll(dbItem->takeChildren());

            auto *tablesFolder = new QTreeWidgetItem(dbItem);
            tablesFolder->setText(0, "Tables");
            tablesFolder->setData(0, RoleType, "folder");

            for (const QStringList& row : response.rows) {
                if (row.isEmpty()) {
                    continue;
                }

                const QString tableName = row[0].trimmed();

                if (tableName.isEmpty()) {
                    continue;
                }

                auto *tableItem = new QTreeWidgetItem(tablesFolder);
                tableItem->setText(0, tableName);
                tableItem->setData(0, RoleType, "table");
                tableItem->setData(0, RoleDatabase, m_pendingDatabase);
                tableItem->setData(0, RoleTable, tableName);
            }

            dbItem->setExpanded(true);
            tablesFolder->setExpanded(true);

            m_statusLabel->setText(QString("已加载数据库 %1 的表列表。").arg(m_pendingDatabase));
            appendLog(QString("已加载数据库 %1 的表列表。").arg(m_pendingDatabase));

            m_requestMode = ClientRequestMode::None;
            return;
        }

        if (m_requestMode == ClientRequestMode::OpenTableUseDatabase) {
            m_requestMode = ClientRequestMode::OpenTableSelect;

            appendLog(QString("发送 SQL：SELECT * FROM %1;").arg(m_pendingTable));
            m_dbClient->executeSql(QString("SELECT * FROM %1;").arg(m_pendingTable));
            return;
        }

        if (m_requestMode == ClientRequestMode::OpenTableSelect) {
            m_currentDatabase = m_pendingDatabase;
            m_currentTable = m_pendingTable;

            fillDataTable(response.headers, response.rows);
            fillSqlResultTable(response.headers, response.rows);

            m_tabWidget->setCurrentIndex(0);
            updateWindowCaption();

            m_statusLabel->setText(QString("正在加载表结构：%1.%2")
                               .arg(m_currentDatabase, m_currentTable));

            appendLog(QString("发送 SQL：DESC %1;").arg(m_currentTable));

            m_requestMode = ClientRequestMode::OpenTableDesc;
            m_dbClient->executeSql(QString("DESC %1;").arg(m_currentTable));
            return;
            }

    }
    if (m_requestMode == ClientRequestMode::OpenTableDesc) {
    fillStructureTableFromDesc(response.rows);
    m_originalColumns = currentStructureColumns();

    m_requestMode = ClientRequestMode::None;

    m_statusLabel->setText(QString("已从服务端打开表：%1.%2")
                               .arg(m_currentDatabase, m_currentTable));

    appendLog(QString("已从服务端打开表：%1.%2")
                  .arg(m_currentDatabase, m_currentTable));

    return;
}

    

        

    if (m_isSavingStructure) {
    appendLog(response.message);

    if (!response.success) {
        m_isSavingStructure = false;
        m_pendingStructureSql.clear();

        m_statusLabel->setText("表结构保存失败。");
        QMessageBox::warning(this, "保存失败", response.message);
        return;
    }

    startNextStructureSql();
    return;
}

    if (m_isSavingToServer) {
        appendLog(response.message);

        if (!response.success) {
            m_isSavingToServer = false;
            m_pendingSqlQueue.clear();

            m_statusLabel->setText("表数据保存失败。");
            QMessageBox::warning(this, "保存失败", response.message);
            return;
        }

        executeNextQueuedSql();
        return;
    }
    if (!response.database.isEmpty() && m_currentTable.isEmpty()) {
        m_currentDatabase = response.database;
        updateWindowCaption();
    }


    if (m_isSavingTableChanges && !m_isRefreshingAfterSave) {
        appendLog(response.message);
        if (!response.success) {
            m_isSavingTableChanges = false;
            m_pendingSaveSql.clear();
            m_statusLabel->setText("表数据保存失败。");
            QMessageBox::warning(this, "保存失败", response.message);
            return;
        }

        const int finished = m_saveTotalCount - m_pendingSaveSql.size();
        m_statusLabel->setText(QString("正在保存表数据：%1/%2").arg(finished).arg(m_saveTotalCount));
        startNextSaveSql();
        return;
    }

    if (!response.headers.isEmpty()) {
        fillSqlResultTable(response.headers, response.rows);
        const QString selectedTable = extractSimpleSelectTable(m_lastSubmittedSql);
        if (!selectedTable.isEmpty()) {
            m_currentTable = selectedTable;
            m_isLoadingTable = true;
            m_dataTable->clear();
            m_dataTable->setColumnCount(response.headers.size());
            m_dataTable->setRowCount(response.rows.size());
            m_dataTable->setHorizontalHeaderLabels(response.headers);
            for (int row = 0; row < response.rows.size(); ++row) {
                const QStringList& rowData = response.rows[row];
                for (int col = 0; col < response.headers.size(); ++col) {
                    const QString value = col < rowData.size() ? rowData[col] : "";
                    m_dataTable->setItem(row, col, new QTableWidgetItem(value));
                }
            }
            m_dataTable->resizeColumnsToContents();
            m_dataTable->horizontalHeader()->setStretchLastSection(true);
            m_isLoadingTable = false;
            setOriginalDataSnapshot(response.headers, response.rows);
            m_hasUnsavedChanges = false;
            updateWindowCaption();
        }
    } else {
        m_sqlResultTable->clear();
        m_sqlResultTable->setRowCount(0);
        m_sqlResultTable->setColumnCount(0);
    }

    appendLog(response.message);
    if (m_isRefreshingAfterSave) {
        m_isRefreshingAfterSave = false;
        m_pendingRefreshSql.clear();
        m_statusLabel->setText("表数据已保存并刷新。");
        QMessageBox::information(this, "保存成功", "表数据已保存到服务端。");
        return;
    }

    m_statusLabel->setText(response.success ? "SQL 执行完成。" : "SQL 执行失败。");
}

void MainWindow::onServerError(const QString& message)
{
    if (m_loginPending) {
        m_loginPending = false;
        QMessageBox::warning(this, "连接失败", message);
    }
    appendLog(QString("网络/服务端错误：%1").arg(message));
    m_statusLabel->setText("网络/服务端错误。");
}

void MainWindow::onServerDisconnected()
{
    if (!m_loginPending) {
        appendLog("服务端连接已断开。");
        m_statusLabel->setText("连接已断开。");
    }
}

void MainWindow::fillStructureTableFromHeaders(const QStringList& headers)
{
    m_isLoadingTable = true;

    m_structureTable->clear();
    m_structureTable->setColumnCount(6);
    m_structureTable->setRowCount(headers.size());

    m_structureTable->setHorizontalHeaderLabels(
        {"字段名", "类型", "是否为空", "主键", "默认值", "说明"}
    );

    for (int row = 0; row < headers.size(); ++row) {
        m_structureTable->setItem(row, 0, new QTableWidgetItem(headers[row]));
        m_structureTable->setItem(row, 1, new QTableWidgetItem("VARCHAR(50)"));
        m_structureTable->setItem(row, 2, new QTableWidgetItem("YES"));
        m_structureTable->setItem(row, 3, new QTableWidgetItem(""));
        m_structureTable->setItem(row, 4, new QTableWidgetItem("NULL"));
        m_structureTable->setItem(row, 5, new QTableWidgetItem(""));
    }

    m_structureTable->resizeColumnsToContents();
    m_structureTable->horizontalHeader()->setStretchLastSection(true);

    m_isLoadingTable = false;
}

static QString decodeDescType(const QString& typeCode, const QString& param)
{
    const int type = typeCode.toInt();
    const int size = param.toInt();

    switch (type) {
    case 1:
        return "INT";
    case 2:
    return "BOOL";

    case 3:
        return "DOUBLE";
    case 4:
        return QString("VARCHAR(%1)").arg(size > 0 ? size : 50);
    case 5:
        return "DATETIME";
    default:
        return "VARCHAR(50)";
    }
}

void MainWindow::fillStructureTableFromDesc(const QVector<QStringList>& rows)
{
    m_isLoadingTable = true;

    m_structureTable->clear();
    m_structureTable->setColumnCount(6);
    m_structureTable->setRowCount(rows.size());
    m_structureTable->setHorizontalHeaderLabels(
        {"字段名", "类型", "是否为空", "主键", "默认值", "说明"}
    );

    for (int row = 0; row < rows.size(); ++row) {
        const QStringList& descRow = rows[row];

        const QString name = descRow.size() > 1 ? descRow[1].trimmed() : "";
        const QString type = descRow.size() > 2
            ? decodeDescType(descRow[2], descRow.size() > 3 ? descRow[3] : "")
            : "VARCHAR(50)";

        const QString integrities = descRow.size() > 4 ? descRow[4].trimmed() : "";

        m_structureTable->setItem(row, 0, new QTableWidgetItem(name));
        m_structureTable->setItem(row, 1, new QTableWidgetItem(type));
        m_structureTable->setItem(row, 2, new QTableWidgetItem("YES"));
        m_structureTable->setItem(row, 3, new QTableWidgetItem(integrities == "1" ? "PRI" : ""));
        m_structureTable->setItem(row, 4, new QTableWidgetItem("NULL"));
        m_structureTable->setItem(row, 5, new QTableWidgetItem(""));
    }

    m_structureTable->resizeColumnsToContents();
    m_structureTable->horizontalHeader()->setStretchLastSection(true);

    m_isLoadingTable = false;
    m_hasUnsavedChanges = false;
}

QList<ColumnMeta> MainWindow::currentStructureColumns() const
{
    QList<ColumnMeta> columns;

    for (int row = 0; row < m_structureTable->rowCount(); ++row) {
        ColumnMeta column;

        column.name = tableCellText(m_structureTable, row, 0);
        column.type = tableCellText(m_structureTable, row, 1);
        column.nullable = tableCellText(m_structureTable, row, 2);
        column.key = tableCellText(m_structureTable, row, 3);
        column.defaultValue = tableCellText(m_structureTable, row, 4);
        column.comment = tableCellText(m_structureTable, row, 5);

        columns.append(column);
    }

    return columns;
}

QString MainWindow::buildColumnTypeSql(const ColumnMeta& column) const
{
    QString type = column.type.trimmed().toUpper();

    if (type == "INTEGER") {
        type = "INT";
    }

    if (type == "INT" ||
    type == "BOOL" ||
    type == "DOUBLE" ||
    type == "DATETIME" ||
    type.startsWith("VARCHAR(")) {
    return type;
}


    return QString();
}

QStringList MainWindow::buildStructureAlterSql(const QList<ColumnMeta>& oldColumns,
                                               const QList<ColumnMeta>& newColumns) const
{
    QStringList statements;

    QMap<QString, ColumnMeta> oldMap;
    QMap<QString, ColumnMeta> newMap;

    for (const ColumnMeta& column : oldColumns) {
        oldMap[column.name] = column;
    }

    for (const ColumnMeta& column : newColumns) {
    if (column.name.trimmed().isEmpty()) {
        QMessageBox::warning(nullptr, "保存失败", "字段名不能为空。");
        return {};
    }

    if (newMap.contains(column.name)) {
        QMessageBox::warning(nullptr, "保存失败",
                             QString("字段名重复：%1").arg(column.name));
        return {};
    }

    newMap[column.name] = column;
}


    for (const ColumnMeta& oldColumn : oldColumns) {
        if (!newMap.contains(oldColumn.name)) {
            statements << QString("ALTER TABLE %1 DROP COLUMN %2;")
                              .arg(m_currentTable, oldColumn.name);
        }
    }

    for (const ColumnMeta& newColumn : newColumns) {
        if (!isSafeIdentifier(newColumn.name)) {
            QMessageBox::warning(nullptr, "保存失败",
                                 QString("字段名不合法：%1").arg(newColumn.name));
            return {};
        }

        const QString typeSql = buildColumnTypeSql(newColumn);

        if (typeSql.isEmpty()) {
            QMessageBox::warning(nullptr, "保存失败",
                                 QString("字段类型不支持：%1").arg(newColumn.type));
            return {};
        }

        if (!oldMap.contains(newColumn.name)) {
            statements << QString("ALTER TABLE %1 ADD COLUMN %2 %3;")
                              .arg(m_currentTable, newColumn.name, typeSql);
            continue;
        }

        const ColumnMeta oldColumn = oldMap.value(newColumn.name);
        const QString oldTypeSql = buildColumnTypeSql(oldColumn);

        if (oldTypeSql.compare(typeSql, Qt::CaseInsensitive) != 0) {
            statements << QString("ALTER TABLE %1 MODIFY COLUMN %2 %3;")
                              .arg(m_currentTable, newColumn.name, typeSql);
        }
    }

    return statements;
}

void MainWindow::startNextStructureSql()
{
    if (m_pendingStructureSql.isEmpty()) {
        finishStructureSave();
        return;
    }

    const QString sql = m_pendingStructureSql.takeFirst();

    appendLog(QString("保存表结构发出 SQL：%1").arg(sql));
    m_lastSubmittedSql = sql;

    m_dbClient->executeSql(sql);
}

void MainWindow::finishStructureSave()
{
    m_isSavingStructure = false;
    m_hasUnsavedChanges = false;

    m_statusLabel->setText("表结构已保存，正在刷新当前表...");

    QMessageBox::information(this, "保存成功", "表结构已保存到服务端。");

    openTableFromServer(m_currentDatabase, m_currentTable);
}

