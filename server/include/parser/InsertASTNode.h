#ifndef INSERT_AST_NODE_H
#define INSERT_AST_NODE_H

#include "ASTNode.h"
#include <string>
#include <vector>

namespace Parser {

    class InsertASTNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::INSERT; }

        std::string tableName; // 表名
        std::vector<std::string> columns; // 插入的列名
        std::vector<std::unique_ptr<ASTNode>> values; // 插入的值
    };

}
#endif // INSERT_AST_NODE_H