#ifndef CATALOG_MANAGER_IMPL_H
#define CATALOG_MANAGER_IMPL_H

#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
#include <map>
#include <string>
#include <vector>

namespace Catalog {

    class CatalogManagerImpl : public ICatalogManager {
    private:
        // 核心依赖：所有的元数据最终都要通过它写进硬盘
        Storage::IStorageEngine* storageEngine;
        
        // 运行时的数据库上下文（单机 MVP 版的 Session 状态）
        std::string currentDB;

    public:
        // 构造函数：强制注入存储引擎
        explicit CatalogManagerImpl(Storage::IStorageEngine* storage);
        ~CatalogManagerImpl() override;

        // --- 系统初始化与上下文 ---
        bool initSystem() override;
        bool useDatabase(const std::string& dbName) override;
        std::string getCurrentDatabase() override;
        void setCurrentDatabase(const std::string& dbName) override;


        // --- 数据库管理 (操作 ruanko.db) ---
        bool hasDatabase(const std::string& dbName) override;
        Meta::DatabaseBlock getDatabaseMeta(const std::string& dbName) override;
        std::vector<Meta::DatabaseBlock> getAllDatabases() override;
        bool createDatabase(const Meta::DatabaseBlock& dbMeta) override;
        bool dropDatabase(const std::string& dbName) override;

        // --- 表管理 (操作 .tb 文件) ---
        bool hasTable(const std::string& tableName) override;
        Meta::TableBlock getTableMeta(const std::string& tableName) override;
        bool createTable(const Meta::TableBlock& tableMeta, const std::vector<Meta::FieldBlock>& fields) override;
        bool dropTable(const std::string& tableName) override;
        bool updateTableMeta(const std::string& tableName, const Meta::TableBlock& newTableMeta) override;

        // --- 字段管理 (操作 .tdf 文件) ---
        std::vector<Meta::FieldBlock> getFields(const std::string& tableName) override;
        bool updateField(const std::string& tableName, const Meta::FieldBlock& field) override;
        bool addField(const std::string& tableName, const Meta::FieldBlock& field) override;
        bool dropField(const std::string& tableName, const std::string& fieldName) override;

        // --- 约束与索引 (操作 .tic 和 .tid 文件) ---
        std::vector<Meta::IntegrityBlock> getIntegrities(const std::string& tableName) override;
        std::vector<Meta::IndexBlock> getIndices(const std::string& tableName) override;
        bool addIndex(const std::string& tableName, const Meta::IndexBlock& indexMeta) override;
        bool dropIndex(const std::string& tableName, const std::string& indexName) override;
        bool addIntegrity(const std::string& tableName, const Meta::IntegrityBlock& integrityMeta) override;
        bool dropIntegrity(const std::string& tableName, const std::string& integrityName) override;
    };

} 
#endif // CATALOG_MANAGER_IMPL_H