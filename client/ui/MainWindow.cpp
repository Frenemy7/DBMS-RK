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
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_isLoadingTable(false),
      m_hasUnsavedChanges(false),
      m_dbClient(new DbClient(this)),
      m_saveTotalCount(0),
      m_loginPending(false),
      m_isSavingTableChanges(false),
      m_isRefreshingAfterSave(false)
{
    m_connectionName = "本地连接";

    resize(1280, 820);

    createActions();
    createMenus();
    createToolBar();
    createCentralArea();
    createStatusBar();
    createLogDock();

    initMockData();
    loadExplorer();
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
    if (!m_currentDatabase.isEmpty() && !m_currentTable.isEmpty()) {
        openTable(m_currentDatabase, m_currentTable);
        appendLog("刷新当前表。");
        return;
    }

    loadExplorer();
    m_statusLabel->setText("对象浏览器已刷新。");
    appendLog("刷新对象浏览器。");
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
        return;
    }

    if (type == "table") {
        const QString databaseName = item->data(0, RoleDatabase).toString();
        const QString tableName = item->data(0, RoleTable).toString();

        openTable(databaseName, tableName);
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
        QMessageBox::warning(this, "未连接", "请先连接服务端。");
        return;
    }

    const QStringList headers = currentDataHeaders();
    if (headers.isEmpty()) {
        QMessageBox::information(this, "提示", "当前表没有可保存的列。");
        return;
    }

    if (headers.size() != m_originalDataHeaders.size() || headers != m_originalDataHeaders) {
        QMessageBox::warning(this, "保存失败", "表结构或列顺序已变化，请重新查询当前表后再保存。");
        return;
    }

    QStringList sqlList;
    const QString keyColumn = headers.first();

    QMap<QString, QStringList> oldRowsByKey;
    for (const QStringList& oldRow : m_originalDataRows) {
        if (!oldRow.isEmpty()) {
            oldRowsByKey.insert(oldRow.first(), oldRow);
        }
    }

    QSet<QString> currentKeys;
    bool ok = true;

    for (int row = 0; row < m_dataTable->rowCount(); ++row) {
        const QStringList rowData = currentDataRow(row);
        if (rowData.isEmpty() || rowData.first().trimmed().isEmpty()) {
            QMessageBox::warning(this, "保存失败", QString("第 %1 行的 %2 不能为空。").arg(row + 1).arg(keyColumn));
            return;
        }

        const QString key = rowData.first();
        if (currentKeys.contains(key)) {
            QMessageBox::warning(this, "保存失败", QString("主键/首列值重复：%1").arg(key));
            return;
        }
        currentKeys.insert(key);

        if (!oldRowsByKey.contains(key)) {
            sqlList << buildInsertSql(headers, rowData, &ok);
        } else {
            const QString updateSql = buildUpdateSql(headers, oldRowsByKey[key], rowData, &ok);
            if (!updateSql.isEmpty()) {
                sqlList << updateSql;
            }
        }
    }

    for (const QStringList& oldRow : m_originalDataRows) {
        if (!oldRow.isEmpty() && !currentKeys.contains(oldRow.first())) {
            sqlList << buildDeleteSql(keyColumn, oldRow.first(), &ok);
        }
    }

    if (!ok) {
        QMessageBox::warning(this, "保存失败", "当前保存逻辑暂不支持包含英文单引号的单元格值。");
        return;
    }

    if (sqlList.isEmpty()) {
        QMessageBox::information(this, "无需保存", "表数据没有变化。");
        return;
    }

    m_pendingSaveSql = sqlList;
    m_pendingRefreshSql = QString("SELECT * FROM %1;").arg(m_currentTable);
    m_saveTotalCount = m_pendingSaveSql.size();
    m_isSavingTableChanges = true;
    m_isRefreshingAfterSave = false;

    m_statusLabel->setText(QString("正在保存表数据：共 %1 条操作...").arg(m_saveTotalCount));
    startNextSaveSql();
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

    MockTable &table = m_mockDatabases[m_currentDatabase][m_currentTable];
    table.columns.clear();

    for (int row = 0; row < m_structureTable->rowCount(); ++row) {
        ColumnMeta column;

        QTableWidgetItem *nameItem = m_structureTable->item(row, 0);
        QTableWidgetItem *typeItem = m_structureTable->item(row, 1);
        QTableWidgetItem *nullableItem = m_structureTable->item(row, 2);
        QTableWidgetItem *keyItem = m_structureTable->item(row, 3);
        QTableWidgetItem *defaultItem = m_structureTable->item(row, 4);
        QTableWidgetItem *commentItem = m_structureTable->item(row, 5);

        column.name = nameItem ? nameItem->text().trimmed() : "";
        column.type = typeItem ? typeItem->text().trimmed() : "";
        column.nullable = nullableItem ? nullableItem->text().trimmed() : "";
        column.key = keyItem ? keyItem->text().trimmed() : "";
        column.defaultValue = defaultItem ? defaultItem->text().trimmed() : "";
        column.comment = commentItem ? commentItem->text().trimmed() : "";

        if (column.name.isEmpty()) {
            QMessageBox::warning(this, "保存失败", "字段名不能为空。");
            return;
        }

        if (column.type.isEmpty()) {
            QMessageBox::warning(this, "保存失败", "字段类型不能为空。");
            return;
        }

        table.columns.append(column);
    }

    // 同步表数据页的表头
    table.headers.clear();

    for (const ColumnMeta &column : table.columns) {
        table.headers.append(column.name);
    }

    // 调整每一行数据的列数，使其和新表结构一致
    for (QStringList &rowData : table.rows) {
        while (rowData.size() < table.headers.size()) {
            rowData.append("");
        }

        while (rowData.size() > table.headers.size()) {
            rowData.removeLast();
        }
    }

    m_hasUnsavedChanges = false;

    fillDataTable(table);
    fillStructureTable(table);

    m_statusLabel->setText("表结构已保存（模拟保存）。");
    appendLog(QString("保存表结构：%1.%2").arg(m_currentDatabase, m_currentTable));

    QMessageBox::information(
        this,
        "保存成功",
        "当前版本为模拟保存。\n后续接入服务端后，可将该操作转换为 ALTER TABLE 或发送给 server。"
    );
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

    if (m_mockDatabases.contains(databaseName)) {
        QMessageBox::warning(this, "创建失败", "数据库已存在。");
        return;
    }

    // 创建一个空数据库
    m_mockDatabases[databaseName] = QMap<QString, MockTable>();

    loadExplorer();

    m_currentDatabase = databaseName;
    m_currentTable.clear();

    m_statusLabel->setText(QString("数据库 %1 创建成功（模拟）。").arg(databaseName));
    appendLog(QString("新建数据库：%1").arg(databaseName));

    QMessageBox::information(
        this,
        "创建成功",
        QString("数据库 %1 创建成功。\n当前版本为模拟创建，暂未写入服务端。").arg(databaseName)
    );
}

void MainWindow::onDeleteDatabase()
{
    if (m_currentDatabase.isEmpty()) {
        QMessageBox::information(this, "提示", "请先在左侧选择或打开一个数据库。");
        return;
    }

    int ret = QMessageBox::question(
        this,
        "确认删除",
        QString("确定要删除数据库 %1 吗？\n该操作当前为模拟删除。").arg(m_currentDatabase),
        QMessageBox::Yes | QMessageBox::No
    );

    if (ret != QMessageBox::Yes) {
        return;
    }

    QString deletedDatabase = m_currentDatabase;

    m_mockDatabases.remove(deletedDatabase);

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

    loadExplorer();
    updateWindowCaption();

    m_statusLabel->setText(QString("数据库 %1 已删除（模拟）。").arg(deletedDatabase));
    appendLog(QString("删除数据库：%1").arg(deletedDatabase));
}

void MainWindow::onCreateTable()
{
    if (m_currentDatabase.isEmpty()) {
        QMessageBox::information(this, "提示", "请先在左侧选择一个数据库。");
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

    if (m_mockDatabases[m_currentDatabase].contains(tableName)) {
        QMessageBox::warning(this, "创建失败", "当前数据库中已存在同名表。");
        return;
    }

    MockTable newTable;

    newTable.headers = {"id", "name"};

    newTable.columns = {
        {"id", "INT", "NO", "PRI", "NULL", "主键编号"},
        {"name", "VARCHAR(50)", "YES", "", "NULL", "名称"}
    };

    newTable.rows = {};

    m_mockDatabases[m_currentDatabase][tableName] = newTable;

    loadExplorer();
    openTable(m_currentDatabase, tableName);

    m_statusLabel->setText(QString("表 %1 创建成功（模拟）。").arg(tableName));
    appendLog(QString("新建表：%1.%2").arg(m_currentDatabase, tableName));

    QMessageBox::information(
        this,
        "创建成功",
        QString("表 %1 创建成功。\n当前版本为模拟创建，暂未写入服务端。").arg(tableName)
    );
}

void MainWindow::onDeleteTable()
{
    if (m_currentDatabase.isEmpty() || m_currentTable.isEmpty()) {
        QMessageBox::information(this, "提示", "请先在左侧选择一张表。");
        return;
    }

    int ret = QMessageBox::question(
        this,
        "确认删除",
        QString("确定要删除表 %1.%2 吗？\n该操作当前为模拟删除。")
            .arg(m_currentDatabase, m_currentTable),
        QMessageBox::Yes | QMessageBox::No
    );

    if (ret != QMessageBox::Yes) {
        return;
    }

    QString deletedDatabase = m_currentDatabase;
    QString deletedTable = m_currentTable;

    m_mockDatabases[deletedDatabase].remove(deletedTable);

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

    loadExplorer();
    updateWindowCaption();

    m_statusLabel->setText(QString("表 %1.%2 已删除（模拟）。")
                               .arg(deletedDatabase, deletedTable));

    appendLog(QString("删除表：%1.%2").arg(deletedDatabase, deletedTable));
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
}

void MainWindow::onServerResponse(const DbClient::QueryResponse& response)
{
    if (!response.database.isEmpty()) {
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
