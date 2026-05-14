#include "DropDatabaseExecutor.h"
#include <iostream>

namespace Execution {

    DropDatabaseExecutor::DropDatabaseExecutor(std::unique_ptr<Parser::DropDatabaseASTNode> ast,
                                               Catalog::ICatalogManager* catalog)
        : astNode_(std::move(ast)), catalogManager_(catalog) {}

    bool DropDatabaseExecutor::execute() {
        if (!astNode_ || !catalogManager_) return false;

        std::string dbName = astNode_->dbName;

        if (!catalogManager_->hasDatabase(dbName)) {
            std::cerr << "Error: 数据库 '" << dbName << "' 不存在。" << std::endl;
            return false;
        }

        if (!catalogManager_->dropDatabase(dbName)) {
            std::cerr << "Error: 删除数据库 '" << dbName << "' 失败。" << std::endl;
            return false;
        }

        std::cout << "Query OK. 数据库 '" << dbName << "' 已删除。" << std::endl;
        return true;
    }

}
