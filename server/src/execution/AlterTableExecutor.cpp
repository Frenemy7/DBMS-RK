#include "AlterTableExecutor.h"
#include "../../include/common/Constants.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <windows.h>

namespace Execution {

    // 数据类型字符串 → 枚举值
    static int dataTypeToInt(const std::string& typeName) {
        if (typeName == "INT") return static_cast<int>(Common::DataType::INTEGER);
        if (typeName == "VARCHAR" || typeName == "CHAR") return static_cast<int>(Common::DataType::VARCHAR);
        if (typeName == "DOUBLE") return static_cast<int>(Common::DataType::DOUBLE);
        if (typeName == "DATETIME") return static_cast<int>(Common::DataType::DATETIME);
        return static_cast<int>(Common::DataType::VARCHAR);
    }

    AlterTableExecutor::AlterTableExecutor(std::unique_ptr<Parser::AlterTableASTNode> ast,
                                           Catalog::ICatalogManager* catalog)
        : astNode_(std::move(ast)), catalogManager_(catalog) {}

    bool AlterTableExecutor::execute() {
        if (!astNode_ || !catalogManager_) return false;
        if (!catalogManager_->hasTable(astNode_->tableName)) {
            std::cerr << "Error: 表 '" << astNode_->tableName << "' 不存在。" << std::endl;
            return false;
        }
        if (astNode_->alterOp == "ADD")    return executeAddColumn();
        if (astNode_->alterOp == "DROP")   return executeDropColumn();
        if (astNode_->alterOp == "MODIFY") return executeModifyColumn();
        std::cerr << "Error: 不支持的 ALTER TABLE 操作。" << std::endl;
        return false;
    }

    bool AlterTableExecutor::hasRecords(const std::string& tableName) {
        return catalogManager_->getTableMeta(tableName).record_num > 0;
    }

    int AlterTableExecutor::calcFieldSize(const Meta::FieldBlock& f) {
        switch (f.type) {
            case static_cast<int>(Common::DataType::INTEGER):
            case static_cast<int>(Common::DataType::BOOL):   return 4;
            case static_cast<int>(Common::DataType::DOUBLE):  return 8;
            case static_cast<int>(Common::DataType::VARCHAR): {
                int n = (f.param > 0) ? f.param : Common::MAX_PATH_LEN;
                return ((n + 3) / 4) * 4;
            }
            case static_cast<int>(Common::DataType::DATETIME):
                return ((sizeof(SYSTEMTIME) + 3) / 4) * 4;
            default: return 4;
        }
    }

    int AlterTableExecutor::calcRecordSize(const std::vector<Meta::FieldBlock>& fields,
                                            std::vector<int>& offsets) {
        offsets.clear(); offsets.reserve(fields.size());
        int total = 0;
        for (const auto& f : fields) {
            offsets.push_back(total);
            total += calcFieldSize(f);
        }
        return total;
    }

    bool AlterTableExecutor::readTrd(const std::string& path, std::vector<char>& data) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        in.seekg(0, std::ios::end);
        int sz = static_cast<int>(in.tellg());
        in.seekg(0, std::ios::beg);
        data.resize(sz);
        in.read(data.data(), sz);
        return !!in;
    }

    bool AlterTableExecutor::writeTrd(const std::string& path, const char* data, int size) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(data, size);
        return !!out;
    }

    // 需求 3.4.2/3.4.3 — 字段变更时更新全部已有记录
    bool AlterTableExecutor::rewriteRecords(const std::string& tableName,
                                             const std::vector<Meta::FieldBlock>& oldFields,
                                             const std::vector<Meta::FieldBlock>& newFields,
                                             int alterIndex, const std::string& alterOp) {
        Meta::TableBlock tb = catalogManager_->getTableMeta(tableName);
        std::string dbDir = "data/" + catalogManager_->getCurrentDatabase();
        std::string trdPath = dbDir + "/" + std::string(tb.trd);

        // 读取旧 .trd
        std::vector<char> oldData;
        if (!readTrd(trdPath, oldData)) return true; // 无记录则跳过

        // 计算新旧偏移
        std::vector<int> oldOff, newOff;
        int oldRowSize = calcRecordSize(oldFields, oldOff);
        int newRowSize = calcRecordSize(newFields, newOff);
        int rowCount = static_cast<int>(oldData.size()) / oldRowSize;
        if (rowCount == 0) return true;

        // 分配新缓冲区
        std::vector<char> newData(rowCount * newRowSize, 0);

        for (int r = 0; r < rowCount; ++r) {
            const char* src = oldData.data() + r * oldRowSize;
            char* dst = newData.data() + r * newRowSize;

            if (alterOp == "ADD") {
                // 旧字段原样拷贝 + 新列填 0（已在 resize 时清零）
                for (size_t i = 0; i < oldFields.size(); ++i) {
                    std::memcpy(dst + newOff[i], src + oldOff[i], calcFieldSize(oldFields[i]));
                }
            } else if (alterOp == "DROP") {
                // 跳过被删列，其余字段按新偏移拼装
                size_t si = 0; // new field index
                for (size_t oi = 0; oi < oldFields.size(); ++oi) {
                    if (alterIndex >= 0 && static_cast<int>(oi) == alterIndex) continue; // 跳过
                    std::memcpy(dst + newOff[si], src + oldOff[oi], calcFieldSize(oldFields[oi]));
                    si++;
                }
            } else if (alterOp == "MODIFY") {
                // 拷贝所有字段；被修改的列按新宽度截断或填零
                for (size_t i = 0; i < oldFields.size(); ++i) {
                    int oldSz = calcFieldSize(oldFields[i]);
                    int newSz = calcFieldSize(newFields[i]);
                    if (alterIndex < 0 || static_cast<int>(i) != alterIndex) {
                        // 未修改的列：原样拷贝（宽度未变，直接拷即可）
                        std::memcpy(dst + newOff[i], src + oldOff[i], std::min(oldSz, newSz));
                    } else {
                        // 被修改的列：按新宽度截断或填零
                        int copyLen = std::min(oldSz, newSz);
                        std::memcpy(dst + newOff[i], src + oldOff[i], copyLen);
                        // 新宽度更大 → 剩余字节已在 resize 时清零
                    }
                }
            }
        }

        // 写入新 .trd
        if (!writeTrd(trdPath, newData.data(), rowCount * newRowSize)) {
            std::cerr << "Error: 重写记录文件失败。" << std::endl;
            return false;
        }
        return true;
    }

    // ADD COLUMN
    bool AlterTableExecutor::executeAddColumn() {
        auto fields = catalogManager_->getFields(astNode_->tableName);

        for (const auto& f : fields)
            if (astNode_->columnName == std::string(f.name)) {
                std::cerr << "Error: 列 '" << astNode_->columnName << "' 已存在。" << std::endl;
                return false;
            }

        Meta::FieldBlock newField;
        std::memset(&newField, 0, sizeof(newField));
        newField.order = static_cast<int>(fields.size()) + 1;
        std::strncpy(newField.name, astNode_->columnName.c_str(), sizeof(newField.name) - 1);
        newField.type = dataTypeToInt(astNode_->columnType);
        newField.param = astNode_->columnParam;
        GetLocalTime(&newField.mtime);

        // 重写记录：新列默认填零
        auto newFields = fields;
        newFields.push_back(newField);
        std::sort(newFields.begin(), newFields.end(),
                  [](const Meta::FieldBlock& a, const Meta::FieldBlock& b) { return a.order < b.order; });
        if (!rewriteRecords(astNode_->tableName, fields, newFields, -1, "ADD"))
            return false;

        if (!catalogManager_->addField(astNode_->tableName, newField)) {
            std::cerr << "Error: 添加列 '" << astNode_->columnName << "' 失败。" << std::endl;
            return false;
        }

        std::cout << "Query OK. 列 '" << astNode_->columnName << "' 已添加到表 '" << astNode_->tableName << "'。" << std::endl;
        return true;
    }

    // DROP COLUMN
    bool AlterTableExecutor::executeDropColumn() {
        auto fields = catalogManager_->getFields(astNode_->tableName);
        int dropIdx = -1;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (astNode_->columnName == std::string(fields[i].name)) { dropIdx = static_cast<int>(i); break; }
        }
        if (dropIdx < 0) {
            std::cerr << "Error: 列 '" << astNode_->columnName << "' 不存在。" << std::endl;
            return false;
        }

        // 重写记录：移除被删列
        auto newFields = fields;
        newFields.erase(newFields.begin() + dropIdx);
        if (!rewriteRecords(astNode_->tableName, fields, newFields, dropIdx, "DROP"))
            return false;

        if (!catalogManager_->dropField(astNode_->tableName, astNode_->columnName)) {
            std::cerr << "Error: 删除列 '" << astNode_->columnName << "' 失败。" << std::endl;
            return false;
        }

        std::cout << "Query OK. 列 '" << astNode_->columnName << "' 已从表 '" << astNode_->tableName << "' 删除。" << std::endl;
        return true;
    }

    // MODIFY COLUMN
    bool AlterTableExecutor::executeModifyColumn() {
        auto fields = catalogManager_->getFields(astNode_->tableName);
        int modIdx = -1;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (astNode_->columnName == std::string(fields[i].name)) { modIdx = static_cast<int>(i); break; }
        }
        if (modIdx < 0) {
            std::cerr << "Error: 列 '" << astNode_->columnName << "' 不存在。" << std::endl;
            return false;
        }

        auto newFields = fields;
        newFields[modIdx].type = dataTypeToInt(astNode_->columnType);
        newFields[modIdx].param = astNode_->columnParam;
        GetLocalTime(&newFields[modIdx].mtime);

        // 重写记录：按新宽度截断或填零
        if (!rewriteRecords(astNode_->tableName, fields, newFields, modIdx, "MODIFY"))
            return false;

        if (!catalogManager_->updateField(astNode_->tableName, newFields[modIdx])) {
            std::cerr << "Error: 修改列 '" << astNode_->columnName << "' 失败。" << std::endl;
            return false;
        }

        std::cout << "Query OK. 列 '" << astNode_->columnName << "' 已修改。" << std::endl;
        return true;
    }

}
