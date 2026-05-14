#ifndef SELECT_EXECUTOR_H
#define SELECT_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/execution/IOperator.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
#include "../../include/index/IIndexManager.h"
#include "../../include/parser/SelectASTNode.h"
#include <memory>
#include <vector>
#include <string>

namespace Execution {

    class SelectExecutor : public IExecutor {
    private:
        std::unique_ptr<Parser::SelectASTNode> astNode;
        Catalog::ICatalogManager* catalogManager;
        Storage::IStorageEngine* storageEngine;
        Index::IIndexManager* indexManager_;
        std::unique_ptr<Index::IIndexManager> ownedIndexManager_;

        Index::IIndexManager* indexManager();

    public:
        SelectExecutor(std::unique_ptr<Parser::SelectASTNode> astNode,
                       Catalog::ICatalogManager* catalog,
                       Storage::IStorageEngine* storage,
                       Index::IIndexManager* index = nullptr);

        ~SelectExecutor() override = default;

        bool execute() override;
    };

}
#endif // SELECT_EXECUTOR_H
