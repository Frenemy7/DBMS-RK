#ifndef DROP_TABLE_EXECUTOR_H
#define DROP_TABLE_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/parser/DropTableASTNode.h"
#include "../../include/catalog/ICatalogManager.h"

namespace Execution {

    // 删除表执行器
    class DropTableExecutor : public IExecutor {
    private:
        std::unique_ptr<Parser::DropTableASTNode> astNode_;
        Catalog::ICatalogManager* catalogManager_;

    public:
        DropTableExecutor(std::unique_ptr<Parser::DropTableASTNode> ast,
                         Catalog::ICatalogManager* catalog);
        ~DropTableExecutor() override = default;
        bool execute() override;
    };

}
#endif
