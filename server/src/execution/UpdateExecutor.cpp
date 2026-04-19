#include "UpdateExecutor.h"
#include <iostream>

namespace Execution {

    UpdateExecutor::UpdateExecutor(Parser::ASTNode* ast, 
                                   Catalog::ICatalogManager* cat, 
                                   Storage::IStorageEngine* stor) 
        : astNode(ast), catalog(cat), storage(stor) {}

    UpdateExecutor::~UpdateExecutor() {}

    bool UpdateExecutor::execute() {
        if (!astNode || !catalog || !storage) return false;

        // TODO: 
        
        return false;
    }

}