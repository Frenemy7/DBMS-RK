#ifndef STORAGE_ENGINE_IMPL_H
#define STORAGE_ENGINE_IMPL_H

// 引入公开的接口契约
#include "../../include/storage/IStorageEngine.h"
#include <fstream> // 整个系统中，只有这个文件允许引入 fstream

namespace Storage {

    // 继承并实现纯虚接口
    class StorageEngineImpl : public IStorageEngine {
    public:
        StorageEngineImpl();
        ~StorageEngineImpl() override;

        // 物理文件生命周期管理
        bool createFile(const std::string& filePath) override;
        bool deleteFile(const std::string& filePath) override;
        bool clearFile(const std::string& filePath) override;
        
        // 目录管理
        bool createDirectory(const std::string& dirPath) override;
        bool deleteDirectory(const std::string& dirPath) override;

        // 绝对偏移量读写
        long getFileSize(const std::string& filePath) override;
        bool readRaw(const std::string& filePath, long offset, int size, char* buffer) override;
        bool writeRaw(const std::string& filePath, long offset, int size, const char* data) override;
        long appendRaw(const std::string& filePath, int size, const char* data) override;
    };

}

#endif // STORAGE_ENGINE_IMPL_H