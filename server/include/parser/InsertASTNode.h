#ifndef INSERT_AST_NODE_H
#define INSERT_AST_NODE_H

#include "ASTNode.h"
#include <string>
#include <vector>

namespace Parser {

    class InsertASTNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::INSERT; }

        std::string tableName;
        std::vector<std::string> columns;
        std::vector<std::unique_ptr<ASTNode>> values;     // INSERT ... VALUES (...)
        std::unique_ptr<ASTNode> selectSource = nullptr;   // INSERT ... SELECT ...（非空则为 SELECT 语句）
    };

}
#endif // INSERT_AST_NODE_H