#include "UpdateExecutor.h"
#include "operators/SeqScanOperator.h"
#include "operators/FilterOperator.h"
#include "IndexMaintenance.h"
#include "../index/IndexManagerImpl.h"
#include "../../include/common/Constants.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <functional>
#include <windows.h>

namespace Execution {

    UpdateExecutor::UpdateExecutor(std::unique_ptr<Parser::UpdateASTNode> ast,
                                   Catalog::ICatalogManager* cat,
                                   Storage::IStorageEngine* stor,
                                   Integrity::IIntegrityManager* integ,
                                   Index::IIndexManager* index)
        : astNode(std::move(ast)), catalog(cat), storage(stor), integrity(integ), indexManager_(index) {}

    UpdateExecutor::~UpdateExecutor() {}

    Index::IIndexManager* UpdateExecutor::indexManager() {
        if (indexManager_) return indexManager_;
        if (!ownedIndexManager_) {
            ownedIndexManager_ = std::make_unique<Index::IndexManagerImpl>(storage);
        }
        return ownedIndexManager_.get();
    }

    int UpdateExecutor::findCol(const std::string& name,
                                 const std::vector<Meta::FieldBlock>& schema) {
        for (size_t i = 0; i < schema.size(); ++i)
            if (std::string(schema[i].name) == name) return static_cast<int>(i);
        return -1;
    }

    // 表达式求值：支持列引用、字面量、算术运算
    std::string UpdateExecutor::evalExpr(Parser::ASTNode* node,
                                          const std::vector<std::string>& row,
                                          const std::vector<Meta::FieldBlock>& schema) {
        if (!node) return "";
        if (auto col = dynamic_cast<Parser::ColumnRefNode*>(node)) {
            int idx = findCol(col->columnName, schema);
            return (idx >= 0 && idx < (int)row.size()) ? row[idx] : "";
        }
        if (auto lit = dynamic_cast<Parser::LiteralNode*>(node))
            return lit->value;
        if (auto bin = dynamic_cast<Parser::BinaryOperatorNode*>(node)) {
            if (bin->op == "+" || bin->op == "-" || bin->op == "*" || bin->op == "/") {
                std::string lv = evalExpr(bin->left.get(), row, schema);
                std::string rv = evalExpr(bin->right.get(), row, schema);
                try {
                    double ln = std::stod(lv), rn = std::stod(rv);
                    if (bin->op == "*") return std::to_string(ln * rn);
                    if (bin->op == "/") return rn != 0 ? std::to_string(ln / rn) : "0";
                    if (bin->op == "+") return std::to_string(ln + rn);
                    if (bin->op == "-") return std::to_string(ln - rn);
                } catch (...) { return "0"; }
            }
        }
        return "";
    }

    // 计算字段的二进制大小
    static int calcFieldSize(const Meta::FieldBlock& f) {
        switch (f.type) {
            case static_cast<int>(Common::DataType::INTEGER):
            case static_cast<int>(Common::DataType::BOOL):    return 4;
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

    // 将字符串值按字段类型写入二进制缓冲区
    static void encodeValue(const std::string& val, int fieldType,
                            char* dst, int fieldSize) {
        std::memset(dst, 0, fieldSize);
        if (fieldType == static_cast<int>(Common::DataType::INTEGER)) {
            int v = 0; try { v = std::stoi(val); } catch (...) {}
            std::memcpy(dst, &v, sizeof(int));
        } else if (fieldType == static_cast<int>(Common::DataType::DOUBLE)) {
            double v = 0; try { v = std::stod(val); } catch (...) {}
            std::memcpy(dst, &v, sizeof(double));
        } else if (fieldType == static_cast<int>(Common::DataType::VARCHAR)) {
            int copyLen = std::min(static_cast<int>(val.size()), fieldSize);
            std::memcpy(dst, val.c_str(), copyLen);
        } else if (fieldType == static_cast<int>(Common::DataType::DATETIME)) {
            SYSTEMTIME st; std::memset(&st, 0, sizeof(st));
            std::sscanf(val.c_str(), "%hu-%hu-%hu", &st.wYear, &st.wMonth, &st.wDay);
            std::memcpy(dst, &st, sizeof(SYSTEMTIME));
        }
    }

    bool UpdateExecutor::execute() {
        if (!astNode || !catalog || !storage || !integrity) return false;

        std::string tableName = astNode->tableName;
        if (!catalog->hasTable(tableName)) {
            std::cerr << "Error: 表 '" << tableName << "' 不存在。" << std::endl;
            return false;
        }

        auto fields = catalog->getFields(tableName);
        std::sort(fields.begin(), fields.end(),
                  [](const Meta::FieldBlock& a, const Meta::FieldBlock& b) { return a.order < b.order; });

        // 计算物理布局
        std::vector<int> offsets; int rowSize = 0;
        for (const auto& f : fields) {
            offsets.push_back(rowSize);
            rowSize += calcFieldSize(f);
        }

        // 读取 .trd 文件
        Meta::TableBlock tb = catalog->getTableMeta(tableName);
        std::string trdPath = "data/" + catalog->getCurrentDatabase() + "/" + std::string(tb.trd);

        std::ifstream in(trdPath, std::ios::binary);
        if (!in) { std::cout << "Query OK. 0 rows affected." << std::endl; return true; }
        in.seekg(0, std::ios::end);
        int fileSize = static_cast<int>(in.tellg());
        in.seekg(0, std::ios::beg);
        if (fileSize == 0) { std::cout << "Query OK. 0 rows affected." << std::endl; return true; }

        std::vector<char> buffer(fileSize);
        in.read(buffer.data(), fileSize);
        in.close();

        int rowCount = fileSize / rowSize;
        int affected = 0;
        std::vector<std::vector<std::string>> finalRows;
        finalRows.reserve(rowCount);

        // 对每行：解码 → 判断 WHERE → 计算 SET → 编码回写
        for (int r = 0; r < rowCount; ++r) {
            char* recordPtr = buffer.data() + r * rowSize;

            // 解码为字符串行
            std::vector<std::string> row(fields.size());
            for (size_t i = 0; i < fields.size(); ++i) {
                const char* src = recordPtr + offsets[i];
                int sz = calcFieldSize(fields[i]);
                if (fields[i].type == static_cast<int>(Common::DataType::INTEGER)) {
                    int v; std::memcpy(&v, src, sizeof(int)); row[i] = std::to_string(v);
                } else if (fields[i].type == static_cast<int>(Common::DataType::DOUBLE)) {
                    double v; std::memcpy(&v, src, sizeof(double)); row[i] = std::to_string(v);
                } else if (fields[i].type == static_cast<int>(Common::DataType::VARCHAR)) {
                    row[i] = std::string(src, strnlen(src, sz));
                } else if (fields[i].type == static_cast<int>(Common::DataType::DATETIME)) {
                    SYSTEMTIME st; std::memcpy(&st, src, sizeof(st));
                    char buf[32]; std::sprintf(buf, "%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
                    row[i] = buf;
                }
            }

            // WHERE 过滤
            bool match = true;
            if (astNode->whereExpressionTree) {
                // 简单条件评估（等值比较 + AND）
                std::function<bool(Parser::ASTNode*)> evalWhere =
                    [&](Parser::ASTNode* cond) -> bool {
                    if (!cond) return true;
                    if (auto bin = dynamic_cast<Parser::BinaryOperatorNode*>(cond)) {
                        if (bin->op == "AND")
                            return evalWhere(bin->left.get()) && evalWhere(bin->right.get());
                        if (bin->op == "OR")
                            return evalWhere(bin->left.get()) || evalWhere(bin->right.get());
                        std::string lv = evalExpr(bin->left.get(), row, fields);
                        std::string rv = evalExpr(bin->right.get(), row, fields);
                        try {
                            double ln = std::stod(lv), rn = std::stod(rv);
                            if (bin->op == "=")  return fabs(ln - rn) < 1e-9;
                            if (bin->op == ">")  return ln > rn;
                            if (bin->op == "<")  return ln < rn;
                            if (bin->op == ">=") return ln >= rn;
                            if (bin->op == "<=") return ln <= rn;
                            if (bin->op == "!=" || bin->op == "<>") return fabs(ln - rn) >= 1e-9;
                        } catch (...) {}
                        if (bin->op == "=")  return lv == rv;
                        if (bin->op == "!=" || bin->op == "<>") return lv != rv;
                    }
                    return false;
                };
                match = evalWhere(astNode->whereExpressionTree.get());
            }

            if (!match) continue;

            // 计算 SET 新值
            for (const auto& sv : astNode->setValues) {
                int idx = findCol(sv.field, fields);
                if (idx < 0) continue;
                std::string newVal = evalExpr(sv.valueExpr.get(), row, fields);
                // 完整性校验
                if (!integrity->checkUpdate(tableName, sv.field, newVal)) {
                    std::cerr << "Error: 更新违反完整性约束。" << std::endl;
                    return false;
                }
                // 编码回写
                encodeValue(newVal, fields[idx].type,
                            recordPtr + offsets[idx], calcFieldSize(fields[idx]));
            }
            affected++;
        }

        if (affected > 0) {
            finalRows.clear();
            finalRows.reserve(rowCount);
            for (int r = 0; r < rowCount; ++r) {
                char* recordPtr = buffer.data() + r * rowSize;
                std::vector<std::string> row(fields.size());
                for (size_t i = 0; i < fields.size(); ++i) {
                    const char* src = recordPtr + offsets[i];
                    int sz = calcFieldSize(fields[i]);
                    if (fields[i].type == static_cast<int>(Common::DataType::INTEGER)) {
                        int v; std::memcpy(&v, src, sizeof(int)); row[i] = std::to_string(v);
                    } else if (fields[i].type == static_cast<int>(Common::DataType::DOUBLE)) {
                        double v; std::memcpy(&v, src, sizeof(double)); row[i] = std::to_string(v);
                    } else if (fields[i].type == static_cast<int>(Common::DataType::VARCHAR)) {
                        row[i] = std::string(src, strnlen(src, sz));
                    } else if (fields[i].type == static_cast<int>(Common::DataType::BOOL)) {
                        int v; std::memcpy(&v, src, sizeof(int)); row[i] = v ? "TRUE" : "FALSE";
                    } else if (fields[i].type == static_cast<int>(Common::DataType::DATETIME)) {
                        SYSTEMTIME st; std::memcpy(&st, src, sizeof(st));
                        char buf[64]; std::sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
                                                   st.wYear, st.wMonth, st.wDay,
                                                   st.wHour, st.wMinute, st.wSecond);
                        row[i] = buf;
                    }
                }
                finalRows.push_back(row);
            }

            Index::IIndexManager* manager = indexManager();
            if (manager && !IndexMaintenance::canRebuildTableIndexesFromRows(
                    tableName, catalog, storage, manager, finalRows)) {
                std::cerr << "Error: update violates unique index." << std::endl;
                return false;
            }

            std::ofstream out(trdPath, std::ios::binary | std::ios::trunc);
            out.write(buffer.data(), fileSize);
            out.close();

            if (manager && !IndexMaintenance::rebuildTableIndexes(tableName, catalog, storage, manager)) {
                std::cerr << "Error: index rebuild failed after update." << std::endl;
                return false;
            }
        }

        std::cout << "Query OK. " << affected << " rows affected." << std::endl;
        return true;
    }

}
