#ifndef DELETE_EXECUTOR_H
#define DELETE_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
#include "../../include/integrity/IIntegrityManager.h"
#include "../../include/parser/ASTNode.h"
#include <memory>
#include <string>

namespace Execution {
    class DeleteExecutor : public IExecutor {
    private:
        std::unique_ptr<Parser::DeleteASTNode> astNode_;
        Catalog::ICatalogManager* catalogManager_;
        Storage::IStorageEngine* storageEngine_;
        Integrity::IIntegrityManager* integrityManager_;

    public:
        DeleteExecutor(std::unique_ptr<Parser::DeleteASTNode> ast,
                       Catalog::ICatalogManager* catalog,
                       Storage::IStorageEngine* storage,
                       Integrity::IIntegrityManager* integrity);
        bool execute() override;
    };
}
#endif