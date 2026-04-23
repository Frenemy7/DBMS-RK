#ifndef DROP_TABLE_AST_NODE_H
#define DROP_TABLE_AST_NODE_H

#include "ASTNode.h"
#include <string>

namespace Parser {

    class DropTableASTNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::DROP_TABLE; }

        std::string tableName; // 表名
    };

}
#endif // DROP_TABLE_AST_NODE_H