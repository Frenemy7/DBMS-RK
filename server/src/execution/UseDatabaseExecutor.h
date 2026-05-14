#ifndef USE_DATABASE_EXECUTOR_H
#define USE_DATABASE_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/parser/ASTNode.h"
#include <memory>

namespace Execution {
    class UseDatabaseExecutor : public IExecutor {
    private:
        std::unique_ptr<Parser::UseDatabaseASTNode> astNode_;
        Catalog::ICatalogManager* catalogManager_;
    public:
        UseDatabaseExecutor(std::unique_ptr<Parser::UseDatabaseASTNode> ast, Catalog::ICatalogManager* catalog);
        bool execute() override;
    };
}
#endif