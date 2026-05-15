#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include "../network/DbClient.h"

class QAction;
class QDockWidget;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QStatusBar;
class QTabWidget;
class QTableWidget;
class QTextEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;

struct ColumnMeta
{
    QString name;
    QString type;
    QString nullable;
    QString key;
    QString defaultValue;
    QString comment;
};

struct MockTable
{
    QStringList headers;
    QList<QStringList> rows;
    QList<ColumnMeta> columns;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onNewConnection();
    void onRefresh();
    void onTreeItemClicked(QTreeWidgetItem *item, int column);
    void onExecuteSql();
    void onClearSql();
    void onAddRow();
    void onDeleteRow();
    void onSaveChanges();
    void onDataCellChanged(int row, int column);
    void onAbout();
    void onAddColumn();
    void onDeleteColumn();
    void onSaveStructure();
    void onStructureCellChanged(int row, int column);
    void onCreateDatabase();
    void onDeleteDatabase();
    void onCreateTable();
    void onDeleteTable();
    void onServerConnected();
    void onServerLoginSucceeded();
    void onServerResponse(const DbClient::QueryResponse& response);
    void onServerError(const QString& message);
    void onServerDisconnected();

private:
    void createActions();
    void createMenus();
    void createToolBar();
    void createCentralArea();
    void createStatusBar();
    void createLogDock();

    void initMockData();
    void loadExplorer();
    void openTable(const QString &databaseName, const QString &tableName);
    void fillDataTable(const MockTable &table);
    void fillStructureTable(const MockTable &table);
    void fillSqlResultTable(const QString &tableName);
    void fillSqlResultTable(const QStringList& headers, const QVector<QStringList>& rows);
    void appendLog(const QString &message);
    void updateWindowCaption();
    QStringList currentDataHeaders() const;
    QStringList currentDataRow(int row) const;
    QString sqlLiteral(const QString& value, bool* ok) const;
    QString extractSimpleSelectTable(const QString& sql) const;
    QString buildInsertSql(const QStringList& headers, const QStringList& row, bool* ok) const;
    QString buildDeleteSql(const QString& keyColumn, const QString& keyValue, bool* ok) const;
    QString buildUpdateSql(const QStringList& headers, const QStringList& oldRow, const QStringList& newRow, bool* ok) const;
    void setOriginalDataSnapshot(const QStringList& headers, const QVector<QStringList>& rows);
    void startNextSaveSql();
    void finishTableSave();

private:
    QAction *m_newConnectionAction;
    QAction *m_refreshAction;
    QAction *m_executeSqlAction;
    QAction *m_aboutAction;
    QAction *m_exitAction;
    QAction *m_createDatabaseAction;
    QAction *m_deleteDatabaseAction;
    QAction *m_createTableAction;
    QAction *m_deleteTableAction;

    QTreeWidget *m_databaseTree;
    QTabWidget *m_tabWidget;

    QTableWidget *m_dataTable;
    QTableWidget *m_structureTable;

    QPushButton *m_addColumnButton;
    QPushButton *m_deleteColumnButton;
    QPushButton *m_saveStructureButton;
    QPlainTextEdit *m_sqlEditor;
    QTableWidget *m_sqlResultTable;

    QPushButton *m_addRowButton;
    QPushButton *m_deleteRowButton;
    QPushButton *m_saveButton;
    QPushButton *m_refreshButton;

    QTextEdit *m_logTextEdit;
    QLabel *m_statusLabel;

    QString m_connectionName;
    QString m_currentDatabase;
    QString m_currentTable;

    QMap<QString, QMap<QString, MockTable>> m_mockDatabases;
    DbClient *m_dbClient;
    QStringList m_originalDataHeaders;
    QVector<QStringList> m_originalDataRows;
    QStringList m_pendingSaveSql;
    QString m_pendingRefreshSql;
    QString m_lastSubmittedSql;
    int m_saveTotalCount;

    bool m_isLoadingTable;
    bool m_hasUnsavedChanges;
    bool m_loginPending;
    bool m_isSavingTableChanges;
    bool m_isRefreshingAfterSave;
};

#endif
