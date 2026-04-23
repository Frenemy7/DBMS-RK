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
        INSERT,
        SELECT,
        UPDATE,
        DELETE_,
        UNKNOWN
    };

    // 专门用于存储列定义的结构体
    struct ColumnDefinition {
        std::string name;       // 列名
        std::string type;       // 数据类型 (INTEGER, VARCHAR 等)
        int param;              // 类型参数 (如 VARCHAR(64) 中的 64)
        bool isPrimaryKey;
        bool isNotNull;
        bool isUnique;

        ColumnDefinition(std::string n = "", std::string t = "", int p = 0) 
            : name(n), type(t), param(p), isPrimaryKey(false), isNotNull(false), isUnique(false) {}
    };

    // 抽象语法树节点基类 这是具体节点的总的基类。其他具体节点继承该基类并进行延展
    class ASTNode {
    public:
        virtual SQLType getType() const = 0; //获取语句类型。虚函数，由对应的具体节点分别实现，返回对应的具体节点类型

        virtual ~ASTNode() = default;
    };

    // 字段引用节点 用于表示 A.name 或 B.score。

    class ColumnRefNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::UNKNOWN; } // 表达式通常不属于某条特定的SQL语句类型

        std::string tableName;  // 表名
        std::string columnName; // 列名
    };
    
    // 聚合函数节点 用于表示 COUNT(B.score) 或 MAX(age) 等

    class FunctionCallNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::UNKNOWN; }

        std::string functionName; // 函数名
        std::vector<std::unique_ptr<ASTNode>> arguments; // 参数
    };

    // 二元操作符节点 WHERE age > 20 这个二元运算

    class BinaryOperatorNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::UNKNOWN; }

        std::string op; // 操作符
        std::unique_ptr<ASTNode> left;  // 左表达式
        std::unique_ptr<ASTNode> right; // 右表达式
    };

    // 多表连接 JOIN

    class JoinNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::UNKNOWN; }

        std::unique_ptr<ASTNode> leftTable;  // 左表 (如 student A)
        std::string joinType;                // 如 "INNER JOIN", "LEFT JOIN"
        std::unique_ptr<ASTNode> rightTable; // 右表 (如 sc B)
        std::unique_ptr<ASTNode> onCondition; // 连接条件 
    };
    
    // 字面量节点（用于包装 SQL 语句中出现的具体数字或字符串常量，如 20, 'Tom'）
    class LiteralNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::UNKNOWN; }

        std::string value; // 常量的具体值（如果是数字就存数字字符串，字符串就存文本）
    };

    class TableNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::UNKNOWN; }
        std::string tableName; // 物理表名
        std::string alias;     // 别名 (可选)
    };

} // namespace Parser

#endif // AST_NODE_H