#ifndef AST_NODE_H
#define AST_NODE_H

#include <string>
#include <vector>
#include <memory>

namespace Parser {

    // SQL 语句类型枚举
    enum class SQLType {
        CREATE_TABLE,
        DROP_TABLE,
        CREATE_INDEX,
        DROP_INDEX,
        INSERT,
        SELECT,
        UPDATE,
        DELETE_,
        CREATE_DATABASE,
        DROP_DATABASE,
        ALTER_TABLE,
        BACKUP_DATABASE,
        RESTORE_DATABASE,
        CREATE_USER,
        DROP_USER,
        GRANT_ROLE,
        REVOKE_ROLE,
        USE_DATABASE,
        UNKNOWN
    };

    // 专门用于存储列定义的结构体
    struct ColumnDefinition {
        std::string name;       // 列名
        std::string type;       // 数据类型 (INTEGER, VARCHAR 等)
        int param;              // 类型参数
        bool isPrimaryKey;
        bool isNotNull;
        bool isUnique;
        bool hasDefault = false;
        std::string defaultValue;
        bool hasCheck = false;
        std::string checkCondition;
        bool isIdentity = false;

        ColumnDefinition(std::string n = "", std::string t = "", int p = 0)
            : name(n), type(t), param(p), isPrimaryKey(false), isNotNull(false), isUnique(false) {}
    };

    // 抽象语法树节点基类 这是具体节点的总的基类。其他具体节点继承该基类并进行延展
    class ASTNode {
    public:
        virtual SQLType getType() const = 0; // 获取语句类型。虚函数，由对应的具体节点分别实现，返回对应的具体节点类型

        virtual ~ASTNode() = default;
    };

    // 字段引用节点 用于表示 A.name 或 B.score 或 *
    class ColumnRefNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::UNKNOWN; }
        std::string tableName;  // 表名（可选，无表名时为空）
        std::string columnName; // 列名，或 "*"
    };

    // 聚合/标量函数节点 用于表示 COUNT(B.score) 或 MAX(age) 等
    class FunctionCallNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::UNKNOWN; }
        std::string functionName; // 函数名
        bool isDistinct = false;  // COUNT(DISTINCT ...)
        std::vector<std::unique_ptr<ASTNode>> arguments; // 参数
    };

    // 二元操作符节点 WHERE age > 20 这个二元运算
    class BinaryOperatorNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::UNKNOWN; }
        std::string op; // 操作符
        std::unique_ptr<ASTNode> left;
        std::unique_ptr<ASTNode> right;
    };

    // 多表连接 JOIN
    class JoinNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::UNKNOWN; }
        std::unique_ptr<ASTNode> left;      // 左表源（可以是 TableNode 或 JoinNode）
        std::string joinType;                // "INNER", "LEFT", "RIGHT", "CROSS"
        std::unique_ptr<ASTNode> right;      // 右表源（TableNode 或子查询）
        std::unique_ptr<ASTNode> onCondition; // ON 条件
    };

    // IN 列表：WHERE x IN (1, 2, 3) 中的 (1, 2, 3)
    class InListNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::UNKNOWN; }
        std::vector<std::unique_ptr<ASTNode>> items;
    };

    // 子查询节点：用于 WHERE x IN (SELECT ...) 或 WHERE EXISTS (SELECT ...)
    class SubqueryNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::UNKNOWN; }
        std::unique_ptr<ASTNode> subquery; // 指向一个 SelectASTNode
    };

    // 字面量节点
    class LiteralNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::UNKNOWN; }
        std::string value;
    };

    class TableNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::UNKNOWN; }
        std::string tableName; // 物理表名
        std::string alias;     // 别名 (可选)
    };

    // IS NULL / IS NOT NULL
    class IsNullNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::UNKNOWN; }
        std::unique_ptr<ASTNode> expr;
        bool notNull = false; // true = IS NOT NULL, false = IS NULL
    };

    // BETWEEN: x BETWEEN a AND b
    class BetweenNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::UNKNOWN; }
        std::unique_ptr<ASTNode> expr;
        std::unique_ptr<ASTNode> low;
        std::unique_ptr<ASTNode> high;
    };

    // 备份/还原数据库
    class BackupDatabaseASTNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::BACKUP_DATABASE; }
        std::string dbName;  // 数据库名
        std::string path;    // 备份目标路径
    };

    class RestoreDatabaseASTNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::RESTORE_DATABASE; }
        std::string dbName;
        std::string path;
    };

    // 用户管理
    class CreateUserASTNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::CREATE_USER; }
        std::string username;
        std::string password;
    };

    class DropUserASTNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::DROP_USER; }
        std::string username;
    };

    class GrantRevokeASTNode : public ASTNode {
    public:
        SQLType getType() const override { return mode == "GRANT" ? SQLType::GRANT_ROLE : SQLType::REVOKE_ROLE; }
        std::string mode;     // "GRANT" or "REVOKE"
        std::string role;     // "ADMIN" or "USER"
        std::string username;
    };

    // 用于 建立数据库;
    class CreateDatabaseASTNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::CREATE_DATABASE; }
        std::string dbName; // 数据库名称
    };

    // 用于 修改表;
    class AlterTableASTNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::ALTER_TABLE; }
        std::string tableName;     // 表名
        std::string alterOp;       // "ADD", "DROP", "MODIFY"
        std::string columnName;    // 列名
        std::string columnType;    // 新类型（ADD/MODIFY 时用）
        int columnParam = 0;       // 类型参数（如 VARCHAR(64) 的 64）
    };

    // 用于 删除数据库;
    class DropDatabaseASTNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::DROP_DATABASE; }
        std::string dbName; // 要删除的数据库名称
    };

    // 用于 切换数据库;
    class UseDatabaseASTNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::USE_DATABASE; }
        std::string dbName; // 要切换到的数据库名称
    };

} // namespace Parser

#endif // AST_NODE_H
