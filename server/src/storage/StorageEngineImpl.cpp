#include "StorageEngineImpl.h"
#include <iostream>

namespace Storage {

    StorageEngineImpl::StorageEngineImpl() {}

    StorageEngineImpl::~StorageEngineImpl() {}

    bool StorageEngineImpl::createFile(const std::string& filePath) {
        // TODO: 组员需在此处使用 std::ofstream 创建文件。传入相对路径
        return false; 
    }

    bool StorageEngineImpl::deleteFile(const std::string& filePath) {
        // TODO: 使用 std::remove() 删除指定文件。
        return false;
    }

    bool StorageEngineImpl::writeRaw(const std::string& filePath, long offset, int size, const char* data) {
        // 检查4字节对齐
        if (size % 4 != 0) {
            std::cerr << "错误：存储非四字节对齐！" << std::endl;
            return false;
        }
        
        // TODO: 开启读写流，将所有内容从offset位置写入文件。写入效果为覆盖写入。
        return false;
    }

    bool StorageEngineImpl::createDirectory(const std::string& dirPath) {
        // TODO: 创建一个文件夹。
        return false;
    }

    bool StorageEngineImpl::deleteDirectory(const std::string& dirPath) {
        // TODO: 删除一个文件夹
        return false;
    }

    long StorageEngineImpl::getFileSize(const std::string& filePath) {
        // TODO: 获取整个文件长度
        return 0;
    }

    bool StorageEngineImpl::readRaw(const std::string& filePath, long offset, int size, char* buffer) {
        // TODO: 使用 std::ifstream 以 std::ios::binary 模式打开文件。一定要用这个std::ios::binary，防止四字节独写问题
        return false;
    }

    bool StorageEngineImpl::writeRaw(const std::string& filePath, long offset, int size, const char* data) {
        // 检查4字节对齐
        if (size % 4 != 0) {
            std::cerr << "错误：存储非四字节对齐！" << std::endl;
            return false;
        }
        
        // TODO: 大模型建议是使用 std::fstream 以 std::ios::in | std::ios::out | std::ios::binary 模式打开，跳转到对应的偏移量处进行覆盖读写
        return false;
    }

    long StorageEngineImpl::appendRaw(const std::string& filePath, int size, const char* data) {
        // 检查4字节对齐
        if (size % 4 != 0) {
            std::cerr << "错误：存储非四字节对齐！" << std::endl;
            return -1; // 读写失败，返回-1
        }

        // TODO: 大模型建议使用 std::ofstream 以 std::ios::app | std::ios::binary 模式打开。
        // 在写入之前，先获取当前的末尾偏移量offset，随后写入数据
        // 写入完成后，将这个偏移量 return 给上层执行器。这个offset将传入bpt用于构建B+树。注意：offset是写入前的偏移量！
        return -1;
    }


}
