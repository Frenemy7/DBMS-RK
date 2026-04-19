#include "DeleteExecutor.h"
#include <iostream>

namespace Execution {

    DeleteExecutor::DeleteExecutor(Parser::ASTNode* ast, 
                                   Catalog::ICatalogManager* cat, 
                                   Storage::IStorageEngine* stor) 
        : astNode(ast), catalog(cat), storage(stor) {}

    DeleteExecutor::~DeleteExecutor() {}

    bool DeleteExecutor::execute() {
        if (!astNode || !catalog || !storage) return false;

        // TODO: 
        
        return false;
    }

}