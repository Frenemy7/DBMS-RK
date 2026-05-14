#ifndef INDEX_EXECUTOR_H
#define INDEX_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
#include "../../include/index/IIndexManager.h"
#include "../../include/parser/IndexASTNode.h"

#include <memory>

namespace Execution {

class CreateIndexExecutor : public IExecutor {
private:
    std::unique_ptr<Parser::CreateIndexASTNode> astNode_;
    Catalog::ICatalogManager* catalog_;
    Storage::IStorageEngine* storage_;
    Index::IIndexManager* indexManager_;
    std::unique_ptr<Index::IIndexManager> ownedIndexManager_;

    Index::IIndexManager* indexManager();

public:
    CreateIndexExecutor(std::unique_ptr<Parser::CreateIndexASTNode> ast,
                        Catalog::ICatalogManager* catalog,
                        Storage::IStorageEngine* storage,
                        Index::IIndexManager* indexManager = nullptr);

    bool execute() override;
};

class DropIndexExecutor : public IExecutor {
private:
    std::unique_ptr<Parser::DropIndexASTNode> astNode_;
    Catalog::ICatalogManager* catalog_;
    Storage::IStorageEngine* storage_;
    Index::IIndexManager* indexManager_;
    std::unique_ptr<Index::IIndexManager> ownedIndexManager_;

    Index::IIndexManager* indexManager();

public:
    DropIndexExecutor(std::unique_ptr<Parser::DropIndexASTNode> ast,
                      Catalog::ICatalogManager* catalog,
                      Storage::IStorageEngine* storage,
                      Index::IIndexManager* indexManager = nullptr);

    bool execute() override;
};

} // namespace Execution

#endif // INDEX_EXECUTOR_H
