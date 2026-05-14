#include "DropTableExecutor.h"
#include "../../include/parser/DropTableASTNode.h"
#include <iostream>

namespace Execution {

    DropTableExecutor::DropTableExecutor(std::unique_ptr<Parser::DropTableASTNode> ast,
                                         Catalog::ICatalogManager* catalog)
        : astNode_(std::move(ast)), catalogManager_(catalog) {}

    bool DropTableExecutor::execute() {
        if (!astNode_ || !catalogManager_) return false;

        std::string tableName = astNode_->tableName;

        if (!catalogManager_->hasTable(tableName)) {
            std::cerr << "Error: 表 '" << tableName << "' 不存在。" << std::endl;
            return false;
        }

        if (!catalogManager_->dropTable(tableName)) {
            std::cerr << "Error: 删除表 '" << tableName << "' 失败。" << std::endl;
            return false;
        }

        std::cout << "Query OK. 表 '" << tableName << "' 已删除。" << std::endl;
        return true;
    }

}
