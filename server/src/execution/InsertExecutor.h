#ifndef INSERT_EXECUTOR_H
#define INSERT_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
#include "../../include/parser/ASTNode.h"

namespace Execution {

    class InsertExecutor : public IExecutor {
    private:
        Parser::ASTNode* astNode;
        Catalog::ICatalogManager* catalog;
        Storage::IStorageEngine* storage;

    public:
        InsertExecutor(Parser::ASTNode* ast, 
                       Catalog::ICatalogManager* catalog, 
                       Storage::IStorageEngine* storage);
        
        ~InsertExecutor() override;
        bool execute() override;
    };

}
#endif // INSERT_EXECUTOR_H