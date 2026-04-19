#include "InsertExecutor.h"
#include <iostream>

namespace Execution {

    InsertExecutor::InsertExecutor(Parser::ASTNode* ast, 
                                   Catalog::ICatalogManager* cat, 
                                   Storage::IStorageEngine* stor) 
        : astNode(ast), catalog(cat), storage(stor) {}

    InsertExecutor::~InsertExecutor() {}

    bool InsertExecutor::execute() {
        if (!astNode || !catalog || !storage) return false;

        // TODO: 
        
        return false;
    }

}