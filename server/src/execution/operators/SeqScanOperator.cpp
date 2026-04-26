#include "SeqScanOperator.h"
#include "../../../include/common/Constants.h"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace Execution {

    SeqScanOperator::SeqScanOperator(std::string tableName, Catalog::ICatalogManager* catalog, Storage::IStorageEngine* storage)
        : tableName(tableName), catalog(catalog), storage(storage), 
          currentOffset(0), fileSize(0), recordSize(0), buffer(nullptr) {}

    SeqScanOperator::~SeqScanOperator() {
        if (buffer != nullptr) {
            delete[] buffer;
        }
    }

    bool SeqScanOperator::init() {
        if (!catalog->hasTable(tableName)) return false;

        Meta::TableBlock tableMeta = catalog->getTableMeta(tableName);
        fields = catalog->getFields(tableName);
        
        std::sort(fields.begin(), fields.end(), [](const Meta::FieldBlock& a, const Meta::FieldBlock& b) {
            return a.order < b.order;
        });

        // 算出单行数据的长度
        for (const auto& field : fields) {
            if (field.type == static_cast<int>(Common::DataType::INTEGER) || field.type == static_cast<int>(Common::DataType::BOOL)) {
                recordSize += 4;
            } else if (field.type == static_cast<int>(Common::DataType::VARCHAR)) {
                int size = (field.param > 0) ? field.param : Common::MAX_PATH_LEN;
                recordSize += ((size + 3) / 4) * 4;
            }
        }

        // 把整个 .trd 文件读进内存
        std::string trdPath = "data/" + catalog->getCurrentDatabase() + "/" + std::string(tableMeta.trd);
        fileSize = storage->getFileSize(trdPath);
        
        if (fileSize > 0) {
            buffer = new char[fileSize];
            storage->readRaw(trdPath, 0, fileSize, buffer);
        }
        
        currentOffset = 0;
        return true;
    }

    bool SeqScanOperator::next(std::vector<std::string>& row) {
        if (currentOffset >= fileSize || recordSize == 0 || buffer == nullptr) {
            return false;
        }

        row.clear(); // 清空上一行的数据
        int fieldOffset = 0;

        // 切割当前这一行的数据
        for (const auto& field : fields) {
            char* currentPtr = buffer + currentOffset + fieldOffset;

            if (field.type == static_cast<int>(Common::DataType::INTEGER)) {
                int val;
                std::memcpy(&val, currentPtr, sizeof(int));
                row.push_back(std::to_string(val));
                fieldOffset += 4;
            } else if (field.type == static_cast<int>(Common::DataType::VARCHAR)) {
                int size = (field.param > 0) ? field.param : Common::MAX_PATH_LEN;
                int alignedSize = ((size + 3) / 4) * 4;
                row.push_back(std::string(currentPtr)); 
                fieldOffset += alignedSize;
            } else if (field.type == static_cast<int>(Common::DataType::DOUBLE)) {
                double val;
                std::memcpy(&val, currentPtr, sizeof(double));
                row.push_back(std::to_string(val));
                fieldOffset += 8;
            } else if (field.type == static_cast<int>(Common::DataType::BOOL)) {
                int val;
                std::memcpy(&val, currentPtr, sizeof(int));
                row.push_back(val ? "TRUE" : "FALSE");
                fieldOffset += 4;
            } else if (field.type == static_cast<int>(Common::DataType::DATETIME)) {
                int alignedSize = ((sizeof(SYSTEMTIME) + 3) / 4) * 4;
                SYSTEMTIME st;
                std::memcpy(&st, currentPtr, sizeof(SYSTEMTIME));
                char timeBuf[64];
                std::sprintf(timeBuf, "%04d-%02d-%02d %02d:%02d:%02d", 
                             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
                row.push_back(std::string(timeBuf));
                fieldOffset += alignedSize;
            }
            
        }

        currentOffset += recordSize; 
        return true;
    }

    std::vector<Meta::FieldBlock> SeqScanOperator::getOutputSchema() const {
        return fields;
    }

}