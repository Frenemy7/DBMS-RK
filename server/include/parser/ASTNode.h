#ifndef AST_NODE_H
#define AST_NODE_H

#include <string>
#include <vector>

namespace Parser {

    // SQL 语句类型枚举
    enum class SQLType {
        CREATE_TABLE,
        DROP_TABLE,
        INSERT,
        SELECT,
        UPDATE,
        DELETE,
        UNKNOWN//////////////////////////////////////
    };

    // 抽象语法树节点基类
    class ASTNode {
    public:
        SQLType type;                  // 这句话到底是干什么的
        std::string tableName;         // 涉及的核心表名

        virtual ~ASTNode() = default;
    };

    // ---------------------------------------------------------
    // 下面是具体的子节点（以 Insert 和 Select 为例），供组员扩展
    // ---------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////////////////////
    // 比如：INSERT INTO 表名 VALUES (值1, 值2)
    class InsertASTNode : public ASTNode {
    public:
        std::vector<std::string> values; // 提取出来的插入值
        InsertASTNode() { type = SQLType::INSERT; }
    };

    // 比如：SELECT * FROM 表名 WHERE 字段 = 值
    class SelectASTNode : public ASTNode {
    public:
        std::vector<std::string> fields; // 要查询的字段 (如 "*", "name", "age")
        std::string whereField;          // 条件字段
        std::string whereValue;          // 条件值
        SelectASTNode() { type = SQLType::SELECT; }
    };

} // namespace Parser

#endif // AST_NODE_H