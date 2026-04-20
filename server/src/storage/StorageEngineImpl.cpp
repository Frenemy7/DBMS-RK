#include "StorageEngineImpl.h"
#include <iostream>

#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

namespace Storage {

    StorageEngineImpl::StorageEngineImpl() {}

    StorageEngineImpl::~StorageEngineImpl() {}

    bool StorageEngineImpl::createFile(const std::string& filePath) {
        // TODO: 组员需在此处使用 std::ofstream 创建文件。传入相对路径
        std::ofstream ofs(filePath, std::ios::binary);
        if (!ofs.is_open()) {
            std::cerr << "错误：无法创建文件: " << filePath << std::endl;
            return false;
        }
        ofs.close();
        return true; 
    }

    bool StorageEngineImpl::deleteFile(const std::string& filePath) {
        // TODO: 使用 std::remove() 删除指定文件。
        if (!fs::exists(filePath)) {
            return false;
        }

        std::error_code ec;
        bool removed = fs::remove(filePath, ec);
        if (ec) {
            std::cerr << "错误：删除文件失败: " << filePath
                      << "，原因: " << ec.message() << std::endl;
            return false;
        }
        return removed;
    }

    // bool StorageEngineImpl::writeRaw(const std::string& filePath, long offset, int size, const char* data) {
    //     // 检查4字节对齐
    //     if (size % 4 != 0) {
    //         std::cerr << "错误：存储非四字节对齐！" << std::endl;
    //         return false;
    //     }
        
    //     // TODO: 开启读写流，将所有内容从offset位置写入文件。写入效果为覆盖写入。
    //     return false;
    // }


    bool StorageEngineImpl::clearFile(const std::string& filePath) {
        std::ofstream ofs(filePath, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            std::cerr << "错误：无法清空文件: " << filePath << std::endl;
            return false;
        }
        ofs.close();
        return true;
    }

    bool StorageEngineImpl::createDirectory(const std::string& dirPath) {
        // TODO: 创建一个文件夹。
        std::error_code ec;

        if (fs::exists(dirPath)) {
            return fs::is_directory(dirPath);
        }

        bool created = fs::create_directories(dirPath, ec);
        if (ec) {
            std::cerr << "错误：创建目录失败: " << dirPath
                      << "，原因: " << ec.message() << std::endl;
            return false;
        }
        return created;
    }

    bool StorageEngineImpl::deleteDirectory(const std::string& dirPath) {
        // TODO: 删除一个文件夹
        if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
            return false;
        }

        std::error_code ec;
        fs::remove_all(dirPath, ec);
        if (ec) {
            std::cerr << "错误：删除目录失败: " << dirPath
                      << "，原因: " << ec.message() << std::endl;
            return false;
        }
        return true;
    }

    long StorageEngineImpl::getFileSize(const std::string& filePath) {
        // TODO: 获取整个文件长度
        std::ifstream ifs(filePath, std::ios::binary | std::ios::ate);
        if (!ifs.is_open()) {
            std::cerr << "错误：无法打开文件获取大小: " << filePath << std::endl;
            return -1;
        }

        std::streampos size = ifs.tellg();
        if (size < 0) {
            std::cerr << "错误：获取文件大小失败: " << filePath << std::endl;
            return -1;
        }

        return static_cast<long>(size);
    }

    bool StorageEngineImpl::readRaw(const std::string& filePath, long offset, int size, char* buffer) {
        // TODO: 使用 std::ifstream 以 std::ios::binary 模式打开文件。一定要用这个std::ios::binary，防止四字节独写问题
        if (offset < 0 || size < 0 || buffer == nullptr) {
            std::cerr << "错误：readRaw 参数非法。" << std::endl;
            return false;
        }

        std::ifstream ifs(filePath, std::ios::binary);
        if (!ifs.is_open()) {
            std::cerr << "错误：无法打开文件读取: " << filePath << std::endl;
            return false;
        }

        ifs.seekg(0, std::ios::end);
        long fileSize = static_cast<long>(ifs.tellg());
        if (offset + size > fileSize) {
            std::cerr << "错误：读取越界。fileSize=" << fileSize
                      << ", offset=" << offset
                      << ", size=" << size << std::endl;
            return false;
        }

        ifs.seekg(offset, std::ios::beg);
        if (!ifs.good()) {
            std::cerr << "错误：seekg 失败。" << std::endl;
            return false;
        }

        ifs.read(buffer, size);
        if (!ifs) {
            std::cerr << "错误：读取二进制数据失败。" << std::endl;
            return false;
        }

        return true;
    }

    bool StorageEngineImpl::writeRaw(const std::string& filePath, long offset, int size, const char* data) {
        // 检查4字节对齐
        if (size % 4 != 0) {
            std::cerr << "错误：存储非四字节对齐！" << std::endl;
            return false;
        }

        if (offset < 0 || size < 0 || data == nullptr) {
            std::cerr << "错误：writeRaw 参数非法。" << std::endl;
            return false;
        }

        std::fstream fsStream(filePath, std::ios::in | std::ios::out | std::ios::binary);
        if (!fsStream.is_open()) {
            std::cerr << "错误：无法打开文件进行覆盖写入: " << filePath << std::endl;
            return false;
        }

        fsStream.seekp(0, std::ios::end);
        long fileSize = static_cast<long>(fsStream.tellp());
        if (offset > fileSize) {
            std::cerr << "错误：写入偏移超出文件范围。fileSize=" << fileSize
                      << ", offset=" << offset << std::endl;
            return false;
        }

        fsStream.seekp(offset, std::ios::beg);
        if (!fsStream.good()) {
            std::cerr << "错误：seekp 失败。" << std::endl;
            return false;
        }

        fsStream.write(data, size);
        if (!fsStream) {
            std::cerr << "错误：覆盖写入失败。" << std::endl;
            return false;
        }

        fsStream.flush();
        return true;
        // TODO: 大模型建议是使用 std::fstream 以 std::ios::in | std::ios::out | std::ios::binary 模式打开，跳转到对应的偏移量处进行覆盖读写
        
    }

    long StorageEngineImpl::appendRaw(const std::string& filePath, int size, const char* data) {
        // 检查4字节对齐
        if (size % 4 != 0) {
            std::cerr << "错误：存储非四字节对齐！" << std::endl;
            return -1;
        }

        if (size < 0 || data == nullptr) {
            std::cerr << "错误：appendRaw 参数非法。" << std::endl;
            return -1;
        }

        long offset = getFileSize(filePath);
        if (offset < 0) {
            return -1;
        }

        std::ofstream ofs(filePath, std::ios::binary | std::ios::app);
        if (!ofs.is_open()) {
            std::cerr << "错误：无法打开文件追加写入: " << filePath << std::endl;
            return -1;
        }

        ofs.write(data, size);
        if (!ofs) {
            std::cerr << "错误：追加写入失败。" << std::endl;
            return -1;
        }

        ofs.flush();
        return offset;
        // TODO: 大模型建议使用 std::ofstream 以 std::ios::app | std::ios::binary 模式打开。
        // 在写入之前，先获取当前的末尾偏移量offset，随后写入数据
        // 写入完成后，将这个偏移量 return 给上层执行器。这个offset将传入bpt用于构建B+树。注意：offset是写入前的偏移量！
        
    }


}
