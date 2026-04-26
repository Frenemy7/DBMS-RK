#include "UseDatabaseExecutor.h"
#include <iostream>
#include <filesystem>

namespace Execution {
    UseDatabaseExecutor::UseDatabaseExecutor(std::unique_ptr<Parser::UseDatabaseASTNode> ast, Catalog::ICatalogManager* catalog)
        : astNode_(std::move(ast)), catalogManager_(catalog) {}

    bool UseDatabaseExecutor::execute() {
        if (!astNode_) return false;

        std::string dbPath = "data/" + astNode_->dbName;

        // 切库前，必须检查硬盘上到底有没有这个文件夹
        if (!std::filesystem::exists(dbPath) || !std::filesystem::is_directory(dbPath)) {
            std::cerr << "Error: 数据库 '" << astNode_->dbName << "' 不存在。请先 CREATE DATABASE。" << std::endl;
            return false;
        }

        // 通知 CatalogManager 更新全局上下文
        catalogManager_->setCurrentDatabase(astNode_->dbName);
        std::cout << "Database changed. 当前使用数据库: " << astNode_->dbName << std::endl;
        return true;
    }
}