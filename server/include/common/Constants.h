#ifndef RUANKO_CONSTANTS_H
#define RUANKO_CONSTANTS_H

namespace Common {

    // 目录常量
    const char* const DBMS_ROOT = "data/"; // 物理文件存储根目录

    // 数据大小常量界限 (严格参照文档 3.12 节)
    const int MAX_NAME_LEN = 128;      // 名称最大长度
    const int MAX_PATH_LEN = 256;      // 路径最大长度
    const int PAGE_SIZE = 4096;        // 默认内存页大小(4KB)，专门给组员A写 Buffer 用的

    // 数据库类型枚举
    enum class DatabaseType {
        SYSTEM = 0,
        USER = 1
    };

    // 数据类型枚举 (对应 3.12.1 数据类型)
    enum class DataType {
        INTEGER = 1,
        BOOL = 2,
        DOUBLE = 3,
        VARCHAR = 4,
        DATETIME = 5
    };

    // 全局错误码 (团队统一抛出这些错误，方便前端判断)
    enum class ErrorCode {
        SUCCESS = 0,
        ERR_DB_NOT_EXIST = 1001,
        ERR_TABLE_NOT_EXIST = 1002,
        ERR_FIELD_NOT_EXIST = 1003,
        ERR_DUPLICATE_PK = 2001,   // 主键冲突
        ERR_NULL_CONSTRAINT_VIOLATION = 2002,  // 违反非空约束
        ERR_CHECK_CONSTRAINT_VIOLATION = 2003, // 违反 Check 约束
        ERR_UNIQUE_CONSTRAINT_VIOLATION = 2004,// 违反唯一约束
        ERR_TYPE_MISMATCH = 3002               // 插入数据类型与定义不符 (如把字符串插进 INTEGER 字段)
        ERR_SYNTAX_ERROR = 3001    // 语法错误
    };

} // namespace Common

#endif // RUANKO_CONSTANTS_H