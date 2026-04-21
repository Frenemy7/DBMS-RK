#ifndef RUANKO_TABLE_META_H
#define RUANKO_TABLE_META_H

#include "../common/Constants.h"

#include <windows.h>

namespace Meta {

// 强制编译器按照 4 字节对齐，满足文档要求 3.12.7.3
#pragma pack(push, 4)

    // 数据库基本信息块 (对应 ruanko.db 文件结构) 3.12.3
    struct DatabaseBlock {
        char name[Common::MAX_NAME_LEN];
        int type;   // 对应 Common::DatabaseType
        char filename[Common::MAX_PATH_LEN]; // 数据库目录名或数据库标识
        SYSTEMTIME crtime;
    };

    // 表格信息结构块 (对应 .tb 文件结构) 3.12.5
    struct TableBlock {
        char name[Common::MAX_NAME_LEN];
        int record_num;
        int field_num;
        // 这里只存文件名，不存完整路径；完整路径由 CatalogManager 根据 currentDB 拼接
        char tdf[Common::MAX_PATH_LEN];
        char tic[Common::MAX_PATH_LEN];
        char trd[Common::MAX_PATH_LEN];
        char tid[Common::MAX_PATH_LEN];
        SYSTEMTIME crtime;
        SYSTEMTIME mtime;
    };

    // 字段定义块 (对应 .tdf 文件结构) 3.12.6
    struct FieldBlock {
        int order;
        char name[Common::MAX_NAME_LEN];
        int type;       // 对应 Common::DataType
        int param;      // 比如 VARCHAR(255)，这里存 255
        SYSTEMTIME mtime;
        int integrities;
    };

    // 完整性约束块 (对应 [表名].tic) 3.12.8
    struct IntegrityBlock {
        char name[Common::MAX_NAME_LEN];      // 约束名称 
        char field[Common::MAX_NAME_LEN];     // 涉及的字段名称 
        int type;                              // 约束类型 (如 3.12.2 中的定义) 
        char param[Common::MAX_PATH_LEN];     // 约束参数 
    };

    // 索引块 (对应 [表名].tid)
    struct IndexBlock {
        char name[Common::MAX_NAME_LEN];      // 索引名称 
        bool unique;                           // 是否唯一索引 
        bool asc;                              // 排序方式: true 升序, false 降序 
        int field_num;                         // 索引涵盖的字段数 
        char fields[2][Common::MAX_NAME_LEN]; // 索引关联的字段名 (最多支持2个) 
        char record_file[Common::MAX_PATH_LEN];// 索引对应记录文件路径 
        char index_file[Common::MAX_PATH_LEN]; // 索引数据文件路径 (.ix) 
    };

#pragma pack(pop) // 恢复默认对齐方式

} // namespace Meta

#endif // RUANKO_TABLE_META_H