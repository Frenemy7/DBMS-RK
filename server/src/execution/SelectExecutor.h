#ifndef SELECT_EXECUTOR_H
#define SELECT_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
#include "../../include/parser/ASTNode.h"

namespace Execution {

    class SelectExecutor : public IExecutor {
    private:
        Parser::ASTNode* astNode;
        Catalog::ICatalogManager* catalog;
        Storage::IStorageEngine* storage;

    public:
        SelectExecutor(Parser::ASTNode* ast, 
                       Catalog::ICatalogManager* catalog, 
                       Storage::IStorageEngine* storage);
        
        ~SelectExecutor() override;
        bool execute() override;
    };

}
#endif // SELECT_EXECUTOR_H