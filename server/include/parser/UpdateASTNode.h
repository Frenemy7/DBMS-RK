#ifndef UPDATE_AST_NODE_H
#define UPDATE_AST_NODE_H

#include "ASTNode.h"
#include <string>
#include <vector>

namespace Parser {

    // 封装 SET 子句中的 字段=表达式 键值对
    struct SetClause {
        std::string field;           // 字段名
        std::unique_ptr<ASTNode> valueExpr;      // 值表达式（支持字面量/列引用/算术/子查询）
    };

    class UpdateASTNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::UPDATE; }

        std::string tableName;
        std::vector<SetClause> setValues;    // SET语句
        std::unique_ptr<ASTNode> whereExpressionTree; // where 语句
    };

}
#endif // UPDATE_AST_NODE_H