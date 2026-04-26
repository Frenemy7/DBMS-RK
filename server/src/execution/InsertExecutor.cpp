#include "InsertExecutor.h"
#include "../../include/parser/ASTNode.h"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace Execution {

    InsertExecutor::InsertExecutor(std::unique_ptr<Parser::InsertASTNode> ast,
                                   Catalog::ICatalogManager* catalog,
                                   Storage::IStorageEngine* storage,
                                   Integrity::IIntegrityManager* integrity)
        : astNode(std::move(ast)), catalogManager(catalog), 
          storageEngine(storage), integrityManager(integrity) {}

    bool InsertExecutor::execute() {
        if (!astNode || !catalogManager || !storageEngine || !integrityManager) {
            std::cerr << "[执行错误] InsertExecutor 依赖缺失！" << std::endl;
            return false;
        }

        std::string tableName = astNode->tableName;

        // 检查表是否存在
        if (!catalogManager->hasTable(tableName)) {
            std::cerr << "Error: 表 '" << tableName << "' 不存在。" << std::endl;
            return false;
        }

        // 将 ASTNode 里的值提取为纯 string 数组
        std::vector<std::string> insertValues;
        for (const auto& nodePtr : astNode->values) {
            auto literalNode = dynamic_cast<Parser::LiteralNode*>(nodePtr.get()); // 向下类型转换为字面量节点
            if (literalNode) {
                insertValues.push_back(literalNode->value);
            } else {
                std::cerr << "[执行错误] AST 数中包含非 LiteralNode 的值。" << std::endl;
                return false;
            }
        }

        // 调用完整性校验模块
        bool isLegal = integrityManager->checkInsert(tableName, astNode->columns, insertValues);
        if (!isLegal) {
            std::cerr << "Error: 插入失败！违反了完整性约束或数据类型不匹配。" << std::endl;
            return false;
        }

        std::vector<Meta::FieldBlock> fields = catalogManager->getFields(tableName);
        std::sort(fields.begin(), fields.end(), [](const Meta::FieldBlock& a, const Meta::FieldBlock& b) {
            return a.order < b.order;
        });

        // 计算整行数据的物理字节总大小（4 字节对齐）
        int totalRecordSize = 0;
        for (const auto& field : fields) {
            if (field.type == static_cast<int>(Common::DataType::INTEGER)) {
                totalRecordSize += 4;
            } else if (field.type == static_cast<int>(Common::DataType::VARCHAR)) {
                // 取 param 和 256 的较大者，并强制进位到 4 的倍数
                int size = (field.param > 0) ? field.param : Common::MAX_PATH_LEN;
                int alignedSize = ((size + 3) / 4) * 4; 
                totalRecordSize += alignedSize;
            } else if (field.type == static_cast<int>(Common::DataType::DOUBLE)) {
                // 双精度浮点数占用 8 字节
                totalRecordSize += 8;
            } else if (field.type == static_cast<int>(Common::DataType::DATETIME)) {
                int alignedSize = ((sizeof(SYSTEMTIME) + 3) / 4) * 4;
                totalRecordSize += alignedSize;
            } else if (field.type == static_cast<int>(Common::DataType::BOOL)) {
                totalRecordSize += 4;
            }
            
        }

        // 开辟连续的二进制缓冲区
        char* buffer = new char[totalRecordSize];
        std::memset(buffer, 0, totalRecordSize); 

        // 将字符串强制转换为二进制并写入 Buffer
        int currentOffset = 0;
        for (size_t i = 0; i < fields.size(); ++i) {
            const auto& field = fields[i];
            
            // 缺乏乱序插入判断 ///////////////////////////////////////////////
            if (i >= insertValues.size()) break; 
            std::string rawValue = insertValues[i];

            if (field.type == static_cast<int>(Common::DataType::INTEGER)) {
                // 字符串转 C++ 原生 int
                int intValue = std::stoi(rawValue);
                // 暴力拷贝进内存
                std::memcpy(buffer + currentOffset, &intValue, sizeof(int));
                currentOffset += 4;
            } 
            else if (field.type == static_cast<int>(Common::DataType::VARCHAR)) {
                int size = (field.param > 0) ? field.param : Common::MAX_PATH_LEN;
                int alignedSize = ((size + 3) / 4) * 4;
                
                // 暴力拷贝字符串
                std::strncpy(buffer + currentOffset, rawValue.c_str(), size);
                currentOffset += alignedSize;
            }
        }

        // Storage 追加写入 .trd 文件
        Meta::TableBlock tableMeta = catalogManager->getTableMeta(tableName);
        std::string trdPath = "data/" + catalogManager->getCurrentDatabase() + "/" + std::string(tableMeta.trd);
        
        long writeResult = storageEngine->appendRaw(trdPath, totalRecordSize, buffer);
        
        delete[] buffer; 

        if (writeResult < 0) {
            std::cerr << "Error: 写入硬盘失败！" << std::endl;
            return false;
        }

        tableMeta.record_num += 1;
        catalogManager->updateTableMeta(tableName, tableMeta);

        std::cout << "Query OK. 1 row affected." << std::endl;
        return true;
    }

} // namespace Execution
