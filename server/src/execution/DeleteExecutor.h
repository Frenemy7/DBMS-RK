#ifndef DELETE_EXECUTOR_H
#define DELETE_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
#include "../../include/integrity/IIntegrityManager.h"
#include "../../include/index/IIndexManager.h"
#include "../../include/parser/ASTNode.h"
#include "../../include/parser/DeleteASTNode.h"
#include <memory>
#include <string>

namespace Execution {
    class DeleteExecutor : public IExecutor {
    private:
        std::unique_ptr<Parser::DeleteASTNode> astNode_;
        Catalog::ICatalogManager* catalogManager_;
        Storage::IStorageEngine* storageEngine_;
        Integrity::IIntegrityManager* integrityManager_;
        Index::IIndexManager* indexManager_;
        std::unique_ptr<Index::IIndexManager> ownedIndexManager_;

        Index::IIndexManager* indexManager();

    public:
        DeleteExecutor(std::unique_ptr<Parser::DeleteASTNode> ast,
                       Catalog::ICatalogManager* catalog,
                       Storage::IStorageEngine* storage,
                       Integrity::IIntegrityManager* integrity,
                       Index::IIndexManager* index = nullptr);
        bool execute() override;
    };
}
#endif
