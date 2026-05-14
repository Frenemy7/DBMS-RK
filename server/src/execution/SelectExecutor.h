#ifndef SELECT_EXECUTOR_H
#define SELECT_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/execution/IOperator.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
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

    public:
        SelectExecutor(std::unique_ptr<Parser::SelectASTNode> astNode,
                       Catalog::ICatalogManager* catalog,
                       Storage::IStorageEngine* storage);

        ~SelectExecutor() override = default;

        bool execute() override;
    };

}
#endif // SELECT_EXECUTOR_H
