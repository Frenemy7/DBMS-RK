#ifndef CREATE_TABLE_AST_NODE_H
#define CREATE_TABLE_AST_NODE_H

#include "ASTNode.h"
#include <string>
#include <vector>

namespace Parser {

    class CreateTableASTNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::CREATE_TABLE; }

        std::string tableName; // 表名
        std::vector<ColumnDefinition> columns; // 插入的行
    };

}
#endif // CREATE_TABLE_AST_NODE_H