#include "SelectExecutor.h"
#include <iostream>

namespace Execution {

    SelectExecutor::SelectExecutor(Parser::ASTNode* ast, 
                                   Catalog::ICatalogManager* cat, 
                                   Storage::IStorageEngine* stor) 
        : astNode(ast), catalog(cat), storage(stor) {}

    SelectExecutor::~SelectExecutor() {}

    bool SelectExecutor::execute() {
        if (!astNode || !catalog || !storage) return false;

        // TODO: 
        
        return false;
    }

}