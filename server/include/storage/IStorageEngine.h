#ifndef ISTORAGE_ENGINE_H
#define ISTORAGE_ENGINE_H

#include <string>

namespace Storage {

    // 存储引擎纯虚基类（无缓冲直连硬盘版）
    class IStorageEngine {
    public:
        virtual ~IStorageEngine() = default;

        // 创建空的物理文件 (用于 CREATE TABLE 时生成 .tdf, .trd, .tic, .tid)
        virtual bool createFile(const std::string& filePath) = 0;
        
        // 删除物理文件 (用于 DROP TABLE)
        virtual bool deleteFile(const std::string& filePath) = 0;

        // 创建文件夹 (用于 CREATE DATABASE 时生成数据库专属目录)
        virtual bool createDirectory(const std::string& dirPath) = 0;

        // 删除文件夹 (用于 DROP DATABASE 时连同里面的文件一并清空)
        virtual bool deleteDirectory(const std::string& dirPath) = 0;

        // 清空一个文件的内容，但不删除文件本身 (用于 TRUNCATE TABLE)
        virtual bool clearFile(const std::string& filePath) = 0;

        // 获取当前文件的总字节数
        // 知道文件的末尾在哪里，从而决定全表扫描什么时候结束。
        virtual long getFileSize(const std::string& filePath) = 0;

        // 在指定文件的绝对偏移量 (offset) 处，读取 size 大小的二进制数据
        virtual bool readRaw(const std::string& filePath, long offset, int size, char* buffer) = 0;

        // 在指定文件的绝对偏移量 (offset) 处，覆盖写入 size 大小的二进制数据
        // 负责实现此接口的人，必须在 .cpp 中校验 size % 4 == 0，及必须为4的倍数
        // 注意！！是覆盖！！
        virtual bool writeRaw(const std::string& filePath, long offset, int size, const char* data) = 0;

        // 将数据直接追加到文件末尾，并返回写入成功后的【起始字节偏移量】
        // 就是直接写在文件最后。返回的 offset 将直接交给 Index 模块去构建 B+ 树节点。
        virtual long appendRaw(const std::string& filePath, int size, const char* data) = 0;
    };

} // namespace Storage
#endif