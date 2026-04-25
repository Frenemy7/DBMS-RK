#include "UpdateExecutor.h"
#include "../integrity/IntegrityManagerImpl.h"
#include "../../include/parser/UpdateASTNode.h"
#include <iostream>

namespace Execution {

    UpdateExecutor::UpdateExecutor(Parser::ASTNode* ast, 
                                   Catalog::ICatalogManager* cat, 
                                   Storage::IStorageEngine* stor) 
        : astNode(ast), catalog(cat), storage(stor) {}

    UpdateExecutor::~UpdateExecutor() {}

    bool UpdateExecutor::execute() {
        if (!astNode || !catalog || !storage) return false;

        auto updateNode = dynamic_cast<Parser::UpdateASTNode*>(astNode);
        if (!updateNode) return false;

        Integrity::IntegrityManagerImpl integrityManager(catalog, storage);
        for (const auto& setValue : updateNode->setValues) {
            if (!integrityManager.checkUpdate(updateNode->tableName, setValue.field, setValue.newValue)) {
                return false;
            }
        }

        // TODO:
        
        return false;
    }

}
