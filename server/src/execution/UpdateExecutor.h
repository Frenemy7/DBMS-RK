#ifndef UPDATE_EXECUTOR_H
#define UPDATE_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
#include "../../include/integrity/IIntegrityManager.h"
#include "../../include/parser/ASTNode.h"
#include "../../include/parser/UpdateASTNode.h"
#include <memory>

namespace Execution {

    class UpdateExecutor : public IExecutor {
    private:
        std::unique_ptr<Parser::UpdateASTNode> astNode;
        Catalog::ICatalogManager* catalog;
        Storage::IStorageEngine* storage;
        Integrity::IIntegrityManager* integrity;

    public:
        UpdateExecutor(std::unique_ptr<Parser::UpdateASTNode> astNode, 
                       Catalog::ICatalogManager* catalog, 
                       Storage::IStorageEngine* storage,
                       Integrity::IIntegrityManager* integrity);
        
        ~UpdateExecutor() override;
        bool execute() override;
    };

}
#endif // UPDATE_EXECUTOR_H