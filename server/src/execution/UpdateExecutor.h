#ifndef UPDATE_EXECUTOR_H
#define UPDATE_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
#include "../../include/parser/ASTNode.h"

namespace Execution {

    class UpdateExecutor : public IExecutor {
    private:
        Parser::ASTNode* astNode;
        Catalog::ICatalogManager* catalog;
        Storage::IStorageEngine* storage;

    public:
        UpdateExecutor(Parser::ASTNode* ast, 
                       Catalog::ICatalogManager* catalog, 
                       Storage::IStorageEngine* storage);
        
        ~UpdateExecutor() override;
        bool execute() override;
    };

}
#endif // UPDATE_EXECUTOR_H