#ifndef CREATE_EXECUTOR_H
#define CREATE_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
#include "../../include/parser/CreateTableASTNode.h"
#include <memory>

namespace Execution {

    class CreateExecutor : public IExecutor {
    private:
        std::unique_ptr<Parser::CreateTableASTNode> astNode; // AST
        Catalog::ICatalogManager* catalogManager; // 元数据管理器
        Storage::IStorageEngine* storageEngine; // 存储引擎

    public:
        CreateExecutor(std::unique_ptr<Parser::CreateTableASTNode> astNode, 
                       Catalog::ICatalogManager* catalog, 
                       Storage::IStorageEngine* storage);
        
        ~CreateExecutor() override = default;

        bool execute() override;
    };

}
#endif // CREATE_EXECUTOR_H