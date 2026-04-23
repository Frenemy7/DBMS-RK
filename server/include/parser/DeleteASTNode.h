#ifndef DELETE_AST_NODE_H
#define DELETE_AST_NODE_H

#include "ASTNode.h"
#include <string>
#include <vector>

namespace Parser {

    class DeleteASTNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::DELETE_; }

        std::string tableName;
        std::unique_ptr<ASTNode> whereExpressionTree; // where 语句
    };

}
#endif // DELETE_AST_NODE_H