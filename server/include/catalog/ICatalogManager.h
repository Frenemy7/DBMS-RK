#ifndef ICATALOG_MANAGER_H
#define ICATALOG_MANAGER_H

#include "../meta/TableMeta.h"
#include <string>
#include <vector>

namespace Catalog {

    class ICatalogManager {
    public:
        virtual ~ICatalogManager() = default;

        // 系统初始化
        virtual bool initSystem() = 0;

        // 验证表是否存在 (从 ruanko.db 或 .tb 中查询)
        virtual bool hasTable(const std::string& tableName) = 0;

        // 获取表的完整元数据 (包含记录数、各文件路径等)
        virtual Meta::TableBlock getTableMeta(const std::string& tableName) = 0;

        // 获取某个表的所有字段定义 (从 .tdf 文件读取并按 order 排序)
        virtual std::vector<Meta::FieldBlock> getFields(const std::string& tableName) = 0;

        // 执行 CREATE TABLE 时调用：
        // 由该模块负责在 ruanko.db 中注册，并生成对应的 .tb 和 .tdf 结构写入磁盘
        virtual bool createTable(const Meta::TableBlock& tableMeta, const std::vector<Meta::FieldBlock>& fields) = 0;

        // 执行 DROP TABLE 时调用：注销表信息
        virtual bool dropTable(const std::string& tableName) = 0;
        
        // 动态修改字段时调用 (对应 ALTER TABLE)
        virtual bool updateField(const std::string& tableName, const Meta::FieldBlock& field) = 0;

        // 向 .tdf 文件追加新字段，并使 .tb 文件中的 field_num 加 1
        virtual bool addField(const std::string& tableName, const Meta::FieldBlock& field) = 0;

        // 从 .tdf 文件移除字段，并使 .tb 文件中的 field_num 减 1
        virtual bool dropField(const std::string& tableName, const std::string& fieldName) = 0;

        // 验证数据库是否存在 (查询 ruanko.db)
        virtual bool hasDatabase(const std::string& dbName) = 0;

        // 获取单个数据库的元数据信息
        virtual Meta::DatabaseBlock getDatabaseMeta(const std::string& dbName) = 0;

        // 获取系统中所有数据库的列表 (用于客户端展示)
        virtual std::vector<Meta::DatabaseBlock> getAllDatabases() = 0;

        // 执行 CREATE DATABASE 时调用：在 ruanko.db 中追加记录，并由该函数内部调用 Storage 创建数据文件夹
        virtual bool createDatabase(const Meta::DatabaseBlock& dbMeta) = 0;

        // 执行 DROP DATABASE 时调用：从 ruanko.db 中移除记录
        virtual bool dropDatabase(const std::string& dbName) = 0;

        // 更新表的宏观元数据 (用于 INSERT/DELETE 后更新记录数 record_num，或更新修改时间 mtime)
        virtual bool updateTableMeta(const std::string& tableName, const Meta::TableBlock& newTableMeta) = 0;

        // 获取某个表的所有完整性约束 (从 .tic 文件读取)
        virtual std::vector<Meta::IntegrityBlock> getIntegrities(const std::string& tableName) = 0;

        // 获取某个表的所有索引定义 (从 .tid 文件读取)
        virtual std::vector<Meta::IndexBlock> getIndices(const std::string& tableName) = 0;

        // 切换当前使用的数据库 (对应 USE database_name)
        virtual bool useDatabase(const std::string& dbName) = 0;

        // 获取当前正在使用的数据库名称，供 Executor 拼接文件路径用
        virtual std::string getCurrentDatabase() = 0;

        // 添加一个新的索引 (对应 CREATE INDEX，负责写入 .tid 文件)
        virtual bool addIndex(const std::string& tableName, const Meta::IndexBlock& indexMeta) = 0;

        // 删除一个索引 (对应 DROP INDEX)
        virtual bool dropIndex(const std::string& tableName, const std::string& indexName) = 0;

        // 添加一个新的完整性约束 (对应 ALTER TABLE ADD CONSTRAINT，负责写入 .tic 文件)
        virtual bool addIntegrity(const std::string& tableName, const Meta::IntegrityBlock& integrityMeta) = 0;

        // 删除一个约束 (对应 ALTER TABLE DROP CONSTRAINT)
        virtual bool dropIntegrity(const std::string& tableName, const std::string& integrityName) = 0;
    };

} // namespace Catalog

#endif // ICATALOG_MANAGER_H