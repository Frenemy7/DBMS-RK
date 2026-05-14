#ifndef DROP_DATABASE_EXECUTOR_H
#define DROP_DATABASE_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/parser/ASTNode.h"
#include "../../include/catalog/ICatalogManager.h"
#include <string>

namespace Execution {

    // 删除数据库执行器
    class DropDatabaseExecutor : public IExecutor {
    private:
        std::unique_ptr<Parser::DropDatabaseASTNode> astNode_;
        Catalog::ICatalogManager* catalogManager_;

    public:
        DropDatabaseExecutor(std::unique_ptr<Parser::DropDatabaseASTNode> ast,
                            Catalog::ICatalogManager* catalog);
        ~DropDatabaseExecutor() override = default;
        bool execute() override;
    };

}
#endif
