#ifndef CREATE_DATABASE_EXECUTOR_H
#define CREATE_DATABASE_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/parser/ASTNode.h"
#include <memory>

namespace Execution {
    class CreateDatabaseExecutor : public IExecutor {
    private:
        std::unique_ptr<Parser::CreateDatabaseASTNode> astNode_;
        Catalog::ICatalogManager* catalogManager_;
    public:
        CreateDatabaseExecutor(std::unique_ptr<Parser::CreateDatabaseASTNode> ast, Catalog::ICatalogManager* catalog);
        bool execute() override;
    };
}
#endif