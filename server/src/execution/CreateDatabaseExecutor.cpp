#include "CreateDatabaseExecutor.h" 
#include <iostream>
#include <fstream>
#include <cstring>
#include <windows.h> 

namespace Execution {
    CreateDatabaseExecutor::CreateDatabaseExecutor(std::unique_ptr<Parser::CreateDatabaseASTNode> ast, Catalog::ICatalogManager* catalog)
        : astNode_(std::move(ast)), catalogManager_(catalog) {}

    bool CreateDatabaseExecutor::execute() {
        if (!astNode_) return false;
        std::string dbName = astNode_->dbName;

        Meta::DatabaseBlock dbMeta;
        std::memset(&dbMeta, 0, sizeof(Meta::DatabaseBlock));

        if (dbName.length() >= sizeof(dbMeta.name)) {
            std::cerr << "Error: 数据库名称过长！最大允许 " << (sizeof(dbMeta.name) - 1) << " 个字符。" << std::endl;
            return false;
        }

        if (catalogManager_->hasDatabase(dbName)) {
            std::cerr << "Error: 数据库 '" << dbName << "' 已经存在！" << std::endl;
            return false;
        }

        // 存名字
        std::strncpy(dbMeta.name, dbName.c_str(), sizeof(dbMeta.name) - 1);
        
        // 存类型 (1 代表 USER 数据库，0 代表系统数据库)
        dbMeta.type = 1; 
        
        // 存相对路径
        std::string dbPath = "data/" + dbName;
        std::strncpy(dbMeta.filename, dbPath.c_str(), sizeof(dbMeta.filename) - 1);
        
        // 获取并存入真实的物理创建时间 (调用 Windows API)
        GetLocalTime(&dbMeta.crtime);

        // CatalogManagerImpl::createDatabase() 里面已经调用了 createDirectory
        // 并且安全地把它 appendRaw 到了 ruanko.db 中。
        if (!catalogManager_->createDatabase(dbMeta)) {
            std::cerr << "Error: 创建物理数据库目录或写入 ruanko.db 失败。" << std::endl;
            return false;
        }

        std::string tbPath = dbPath + "/" + dbName + ".tb";
        std::ofstream tbFile(tbPath, std::ios::binary | std::ios::out);
        tbFile.close();

        std::string logPath = dbPath + "/" + dbName + ".log";
        std::ofstream logFile(logPath, std::ios::binary | std::ios::out);
        logFile.close();

        std::cout << "Query OK. 数据库 '" << dbName << "' 创建成功。" << std::endl;
        return true;
    }
}