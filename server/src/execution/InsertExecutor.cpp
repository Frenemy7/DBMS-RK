#include "InsertExecutor.h"
#include "../integrity/IntegrityManagerImpl.h"
#include "../../include/parser/InsertASTNode.h"
#include <iostream>
#include <vector>

namespace {

    bool literalValue(Parser::ASTNode* node, std::string& value) {
        auto literal = dynamic_cast<Parser::LiteralNode*>(node);
        if (!literal) return false;

        value = literal->value;
        return true;
    }

}

namespace Execution {

    InsertExecutor::InsertExecutor(Parser::ASTNode* ast, 
                                   Catalog::ICatalogManager* cat, 
                                   Storage::IStorageEngine* stor) 
        : astNode(ast), catalog(cat), storage(stor) {}

    InsertExecutor::~InsertExecutor() {}

    bool InsertExecutor::execute() {
        if (!astNode || !catalog || !storage) return false;

        auto insertNode = dynamic_cast<Parser::InsertASTNode*>(astNode);
        if (!insertNode) return false;

        std::vector<std::string> values;
        values.reserve(insertNode->values.size());
        for (const auto& valueNode : insertNode->values) {
            std::string value;
            if (!literalValue(valueNode.get(), value)) return false;
            values.push_back(value);
        }

        Integrity::IntegrityManagerImpl integrityManager(catalog, storage);
        if (!integrityManager.checkInsert(insertNode->tableName, insertNode->columns, values)) {
            return false;
        }

        // TODO:
        
        return false;
    }

}
