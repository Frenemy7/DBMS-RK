#ifndef INDEX_AST_NODE_H
#define INDEX_AST_NODE_H

#include "ASTNode.h"

#include <string>
#include <vector>

namespace Parser {

    struct IndexFieldDefinition {
        std::string fieldName;
        bool asc = true;
    };

    class CreateIndexASTNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::CREATE_INDEX; }

        std::string indexName;
        std::string tableName;
        bool unique = false;
        std::vector<IndexFieldDefinition> fields;
    };

    class DropIndexASTNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::DROP_INDEX; }

        std::string indexName;
        std::string tableName;
    };

}

#endif // INDEX_AST_NODE_H
