#include "InsertExecutor.h"
#include "QueryBuilder.h"
#include "../../include/parser/ASTNode.h"
#include "../../include/parser/SelectASTNode.h"
#include <iostream>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <windows.h>

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

        // INSERT ... SELECT ... 分支
        if (astNode->selectSource) {
            // 执行 SELECT 子查询获取结果集
            auto selRows = runSubquery(astNode->selectSource.get(), catalogManager, storageEngine);
            if (selRows.empty()) { std::cout << "Query OK. 0 rows affected." << std::endl; return true; }

            int totalInserted = 0;
            for (const auto& selRow : selRows) {
                std::vector<std::string> insVals;
                for (const auto& v : selRow) insVals.push_back(v);

                // 完整性校验
                if (!integrityManager->checkInsert(tableName, astNode->columns, insVals)) {
                    std::cerr << "Error: INSERT SELECT 违反完整性约束，已回滚。" << std::endl;
                    return false;
                }

                // 复用现有的二进制写入逻辑
                if (!insertSingleRow(tableName, insVals, astNode->columns)) return false;
                totalInserted++;
            }
            std::cout << "Query OK. " << totalInserted << " rows affected." << std::endl;
            return true;
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

        return insertSingleRow(tableName, insertValues, astNode->columns);
    }

    bool InsertExecutor::insertSingleRow(const std::string& tableName,
                                          const std::vector<std::string>& insertValues,
                                          const std::vector<std::string>& insertCols) {
        // 完整性校验
        // 填充默认值：未在 INSERT 列表中出现的列，如果有 DEFAULT 约束则自动补值
        auto enrichedValues = insertValues;
        std::vector<std::string> enrichedCols = insertCols;
        auto integrities = catalogManager->getIntegrities(tableName);
        auto allFields = catalogManager->getFields(tableName);
        for (const auto& ig : integrities) {
            if (ig.type == Integrity::DEFAULT_VALUE) {
                std::string fieldName = ig.field;
                // 检查该列是否已在 INSERT 列表中
                bool alreadySpecified = false;
                for (const auto& c : enrichedCols) if (c == fieldName) { alreadySpecified = true; break; }
                if (alreadySpecified) continue;
                // 添加默认值
                std::string defVal = ig.param;
                if (enrichedCols.empty()) {
                    // 未指定列名：按物理顺序，找到该字段位置插入
                    std::sort(allFields.begin(), allFields.end(),
                        [](const Meta::FieldBlock& a, const Meta::FieldBlock& b) { return a.order < b.order; });
                    size_t pos = 0;
                    for (size_t i = 0; i < allFields.size(); ++i)
                        if (std::string(allFields[i].name) == fieldName) { pos = i; break; }
                    if (pos >= enrichedValues.size()) enrichedValues.resize(pos + 1);
                    enrichedValues[pos] = defVal;
                } else {
                    enrichedCols.push_back(fieldName);
                    enrichedValues.push_back(defVal);
                }
            }
        }

        // IDENTITY 自增：未指定列则 MAX+1
        for (const auto& ig : integrities) {
            if (ig.type == Integrity::IDENTITY) {
                std::string fieldName = ig.field;
                bool already = false;
                for (const auto& c : enrichedCols) if (c == fieldName) { already = true; break; }
                if (already) continue;

                // 从 .trd 文件读取已有数据，计算最大值
                int maxVal = 0;
                Meta::TableBlock tb = catalogManager->getTableMeta(tableName);
                std::string trdPath = "data/" + catalogManager->getCurrentDatabase() + "/" + std::string(tb.trd);
                std::ifstream in(trdPath, std::ios::binary);
                if (in) {
                    in.seekg(0, std::ios::end); int fsz = (int)in.tellg(); in.seekg(0, std::ios::beg);
                    if (fsz > 0) {
                        int rowSize = 0;
                        for (const auto& f : allFields) {
                            if (f.type == (int)Common::DataType::INTEGER || f.type == (int)Common::DataType::BOOL) rowSize += 4;
                            else if (f.type == (int)Common::DataType::DOUBLE) rowSize += 8;
                            else if (f.type == (int)Common::DataType::VARCHAR) { int n = f.param > 0 ? f.param : 256; rowSize += ((n + 3) / 4) * 4; }
                            else if (f.type == (int)Common::DataType::DATETIME) rowSize += ((sizeof(SYSTEMTIME) + 3) / 4) * 4;
                        }
                        int fieldOffset = 0;
                        for (const auto& f : allFields) {
                            if (std::string(f.name) == fieldName) break;
                            if (f.type == (int)Common::DataType::INTEGER || f.type == (int)Common::DataType::BOOL) fieldOffset += 4;
                            else if (f.type == (int)Common::DataType::DOUBLE) fieldOffset += 8;
                            else if (f.type == (int)Common::DataType::VARCHAR) { int n = f.param > 0 ? f.param : 256; fieldOffset += ((n + 3) / 4) * 4; }
                            else if (f.type == (int)Common::DataType::DATETIME) fieldOffset += ((sizeof(SYSTEMTIME) + 3) / 4) * 4;
                        }
                        std::vector<char> buf(fsz); in.read(buf.data(), fsz);
                        int rows = fsz / rowSize;
                        for (int r = 0; r < rows; ++r) {
                            int v; std::memcpy(&v, buf.data() + r * rowSize + fieldOffset, sizeof(int));
                            if (v > maxVal) maxVal = v;
                        }
                    }
                }
                enrichedCols.push_back(fieldName);
                enrichedValues.push_back(std::to_string(maxVal + 1));
            }
        }

        if (!integrityManager->checkInsert(tableName, enrichedCols, enrichedValues)) {
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
            
            // 乱序插入判断
            std::string rawValue = "";
            bool hasValue = false;

            if (enrichedCols.empty()) {
                if (i < enrichedValues.size()) { rawValue = enrichedValues[i]; hasValue = true; }
            } else {
                for (size_t j = 0; j < enrichedCols.size(); ++j) {
                    if (enrichedCols[j] == field.name) { rawValue = enrichedValues[j]; hasValue = true; break; }
                }
            }
            
            if (field.type == static_cast<int>(Common::DataType::INTEGER)) {
                if (hasValue && !rawValue.empty()) {
                    int intValue = std::stoi(rawValue);
                    std::memcpy(buffer + currentOffset, &intValue, sizeof(int));
                }
                currentOffset += 4;
            }
            else if (field.type == static_cast<int>(Common::DataType::BOOL)) {
                if (hasValue && !rawValue.empty()) {
                    std::string upper = rawValue;
                    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
                    int boolValue = (upper == "TRUE" || upper == "1") ? 1 : 0;
                    std::memcpy(buffer + currentOffset, &boolValue, sizeof(int));
                }
                currentOffset += 4;
            }
            else if (field.type == static_cast<int>(Common::DataType::VARCHAR)) {
                int size = (field.param > 0) ? field.param : Common::MAX_PATH_LEN;
                int alignedSize = ((size + 3) / 4) * 4;
                
                if (hasValue && !rawValue.empty()) {
                    // 二进制写入：拷贝实际字符，剩余空间填 0
                    int copyLen = std::min(static_cast<int>(rawValue.length()), size);
                    std::memcpy(buffer + currentOffset, rawValue.c_str(), copyLen);
                    if (copyLen < size) {
                        std::memset(buffer + currentOffset + copyLen, 0, size - copyLen);
                    }
                }
                currentOffset += alignedSize;
            }
            else if (field.type == static_cast<int>(Common::DataType::DOUBLE)) {
                if (hasValue && !rawValue.empty()) {
                    double doubleValue = std::stod(rawValue);
                    std::memcpy(buffer + currentOffset, &doubleValue, sizeof(double));
                }
                currentOffset += 8;
            }
            else if (field.type == static_cast<int>(Common::DataType::DATETIME)) {
                int alignedSize = ((sizeof(SYSTEMTIME) + 3) / 4) * 4; // SYSTEMTIME 通常是 16 字节
                
                if (hasValue && !rawValue.empty()) {
                    SYSTEMTIME st;
                    std::memset(&st, 0, sizeof(SYSTEMTIME)); // 清理脏数据
                    // 提取 YYYY-MM-DD
                    std::sscanf(rawValue.c_str(), "%hu-%hu-%hu", &st.wYear, &st.wMonth, &st.wDay);
                    std::memcpy(buffer + currentOffset, &st, sizeof(SYSTEMTIME));
                }
                currentOffset += alignedSize;
            }
            else {
                // 未知类型兜底：按 4 字节对齐跳过，避免 offset 错位
                currentOffset += 4;
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
