#ifndef DELETE_EXECUTOR_H
#define DELETE_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
#include "../../include/parser/ASTNode.h"

namespace Execution {

    class DeleteExecutor : public IExecutor {
    private:
        Parser::ASTNode* astNode;
        Catalog::ICatalogManager* catalog;
        Storage::IStorageEngine* storage;

    public:
        DeleteExecutor(Parser::ASTNode* ast, 
                       Catalog::ICatalogManager* catalog, 
                       Storage::IStorageEngine* storage);
        
        ~DeleteExecutor() override;
        bool execute() override;
    };

}
#endif // DELETE_EXECUTOR_H