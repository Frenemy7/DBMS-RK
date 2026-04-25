#include "CreateDatabaseExecutor.h"
#include <iostream>
#include <filesystem>

namespace Execution {
    CreateDatabaseExecutor::CreateDatabaseExecutor(std::unique_ptr<Parser::CreateDatabaseASTNode> ast, Catalog::ICatalogManager* catalog)
        : astNode_(std::move(ast)), catalogManager_(catalog) {}

    bool CreateDatabaseExecutor::execute() {
        if (!astNode_) return false;

        // 物理路径映射：直接在 data 目录下建文件夹
        std::string dbPath = "data/" + astNode_->dbName;

        if (std::filesystem::exists(dbPath)) {
            std::cerr << "Error: 数据库 '" << astNode_->dbName << "' 已经存在！" << std::endl;
            return false;
        }

        if (std::filesystem::create_directories(dbPath)) {
            std::cout << "Query OK. 数据库 '" << astNode_->dbName << "' 创建成功。" << std::endl;
            return true;
        } else {
            std::cerr << "Error: 无法在磁盘上创建数据库目录。" << std::endl;
            return false;
        }
    }
}