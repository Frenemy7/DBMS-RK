#include "DeleteExecutor.h"
#include "../integrity/IntegrityManagerImpl.h"
#include "../../include/parser/DeleteASTNode.h"
#include <iostream>

namespace {

    bool extractLiteralEqualityValue(Parser::ASTNode* node, std::string& value) {
        auto binary = dynamic_cast<Parser::BinaryOperatorNode*>(node);
        if (!binary || binary->op != "=") return false;

        auto leftLiteral = dynamic_cast<Parser::LiteralNode*>(binary->left.get());
        auto rightLiteral = dynamic_cast<Parser::LiteralNode*>(binary->right.get());
        auto leftColumn = dynamic_cast<Parser::ColumnRefNode*>(binary->left.get());
        auto rightColumn = dynamic_cast<Parser::ColumnRefNode*>(binary->right.get());

        if (leftColumn && rightLiteral) {
            value = rightLiteral->value;
            return true;
        }

        if (leftLiteral && rightColumn) {
            value = leftLiteral->value;
            return true;
        }

        return false;
    }

}

namespace Execution {

    DeleteExecutor::DeleteExecutor(Parser::ASTNode* ast, 
                                   Catalog::ICatalogManager* cat, 
                                   Storage::IStorageEngine* stor) 
        : astNode(ast), catalog(cat), storage(stor) {}

    DeleteExecutor::~DeleteExecutor() {}

    bool DeleteExecutor::execute() {
        if (!astNode || !catalog || !storage) return false;

        auto deleteNode = dynamic_cast<Parser::DeleteASTNode*>(astNode);
        if (!deleteNode) return false;

        std::string pkValue;
        if (!extractLiteralEqualityValue(deleteNode->whereExpressionTree.get(), pkValue)) {
            return false;
        }

        Integrity::IntegrityManagerImpl integrityManager(catalog, storage);
        if (!integrityManager.checkDelete(deleteNode->tableName, pkValue)) {
            return false;
        }

        // TODO:
        
        return false;
    }

}
