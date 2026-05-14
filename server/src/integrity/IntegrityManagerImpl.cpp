#include "IntegrityManagerImpl.h"

#include "../../include/common/Constants.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

namespace {

    std::string safeString(const char* data, size_t maxLen) {
        size_t len = 0;
        while (len < maxLen && data[len] != '\0') {
            ++len;
        }
        return std::string(data, len);
    }

    std::string trim(const std::string& input) {
        size_t begin = 0;
        while (begin < input.size() && std::isspace(static_cast<unsigned char>(input[begin]))) {
            ++begin;
        }

        size_t end = input.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
            --end;
        }

        return input.substr(begin, end - begin);
    }

    std::string stripQuotes(const std::string& input) {
        std::string value = trim(input);
        if (value.size() >= 2) {
            char first = value.front();
            char last = value.back();
            if ((first == '\'' && last == '\'') || (first == '"' && last == '"')) {
                return value.substr(1, value.size() - 2);
            }
        }
        return value;
    }

    std::string normalizeValue(const std::string& input) {
        std::string value;
        value.reserve(input.size());
        for (char ch : input) {
            if (ch != '\0') {
                value.push_back(ch);
            }
        }
        return stripQuotes(value);
    }

    std::string upperCopy(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        return value;
    }

    bool isNullValue(const std::string& value) {
        std::string normalized = upperCopy(trim(stripQuotes(value)));
        return normalized.empty() || normalized == "NULL";
    }

    bool parseNumber(const std::string& value, double& out) {
        std::string normalized = normalizeValue(value);
        if (normalized.empty()) return false;

        char* end = nullptr;
        out = std::strtod(normalized.c_str(), &end);
        return end != normalized.c_str() && *end == '\0';
    }

    bool valuesEqual(const std::string& left, const std::string& right) {
        std::string a = normalizeValue(left);
        std::string b = normalizeValue(right);

        std::string upperA = upperCopy(a);
        std::string upperB = upperCopy(b);
        if (upperA == "TRUE") a = "1";
        if (upperA == "FALSE") a = "0";
        if (upperB == "TRUE") b = "1";
        if (upperB == "FALSE") b = "0";

        double da = 0.0;
        double db = 0.0;
        if (parseNumber(a, da) && parseNumber(b, db)) {
            return std::fabs(da - db) < 1e-9;
        }

        return a == b;
    }

    std::vector<std::string> splitDelimitedRecord(const std::string& line) {
        char delimiter = ',';
        if (line.find('|') != std::string::npos) delimiter = '|';
        else if (line.find('\t') != std::string::npos) delimiter = '\t';

        std::vector<std::string> parts;
        std::string current;
        bool inSingleQuote = false;
        bool inDoubleQuote = false;

        for (char ch : line) {
            if (ch == '\'' && !inDoubleQuote) {
                inSingleQuote = !inSingleQuote;
            } else if (ch == '"' && !inSingleQuote) {
                inDoubleQuote = !inDoubleQuote;
            }

            if (ch == delimiter && !inSingleQuote && !inDoubleQuote) {
                parts.push_back(normalizeValue(current));
                current.clear();
            } else {
                current.push_back(ch);
            }
        }
        parts.push_back(normalizeValue(current));

        return parts;
    }

    bool looksLikeDelimitedText(const std::vector<char>& buffer) {
        if (buffer.empty()) return false;

        int printable = 0;
        int separators = 0;
        for (char ch : buffer) {
            unsigned char uch = static_cast<unsigned char>(ch);
            if (ch == '\0') return false;
            if (std::isprint(uch) || std::isspace(uch)) ++printable;
            if (ch == '\n' || ch == ',' || ch == '|' || ch == '\t') ++separators;
        }

        return printable == static_cast<int>(buffer.size()) && separators > 0;
    }

    int align4(int size) {
        return ((size + 3) / 4) * 4;
    }

    int fieldSize(const Meta::FieldBlock& field) {
        switch (field.type) {
            case static_cast<int>(Common::DataType::INTEGER):
            case static_cast<int>(Common::DataType::BOOL):
                return 4;
            case static_cast<int>(Common::DataType::DOUBLE):
                return 8;
            case static_cast<int>(Common::DataType::VARCHAR): {
                int size = field.param > 0 ? field.param : Common::MAX_PATH_LEN;
                return align4(size);
            }
            case static_cast<int>(Common::DataType::DATETIME):
                return align4(sizeof(SYSTEMTIME));
            default:
                return align4(Common::MAX_PATH_LEN);
        }
    }

    std::string readBinaryValue(const char* data, const Meta::FieldBlock& field) {
        switch (field.type) {
            case static_cast<int>(Common::DataType::INTEGER): {
                int value = 0;
                std::memcpy(&value, data, sizeof(value));
                return std::to_string(value);
            }
            case static_cast<int>(Common::DataType::BOOL): {
                int value = 0;
                std::memcpy(&value, data, sizeof(value));
                return value ? "1" : "0";
            }
            case static_cast<int>(Common::DataType::DOUBLE): {
                double value = 0.0;
                std::memcpy(&value, data, sizeof(value));
                std::ostringstream oss;
                oss << value;
                return oss.str();
            }
            case static_cast<int>(Common::DataType::VARCHAR): {
                int size = field.param > 0 ? field.param : Common::MAX_PATH_LEN;
                return normalizeValue(std::string(data, data + size));
            }
            default:
                return normalizeValue(std::string(data, data + fieldSize(field)));
        }
    }

    void sortFields(std::vector<Meta::FieldBlock>& fields) {
        std::sort(fields.begin(), fields.end(), [](const Meta::FieldBlock& left,
                                                   const Meta::FieldBlock& right) {
            return left.order < right.order;
        });
    }

}

namespace Integrity {

    IntegrityManagerImpl::IntegrityManagerImpl(Catalog::ICatalogManager* catalogManager,
                                               Storage::IStorageEngine* storageEngine)
        : catalog(catalogManager), storage(storageEngine) {}

    bool IntegrityManagerImpl::checkInsert(const std::string& tableName,
                                           const std::vector<std::string>& columns,
                                           const std::vector<std::string>& values) {
        if (!catalog || !storage || !catalog->hasTable(tableName)) return false;
        if (!columns.empty() && columns.size() != values.size()) return false;

        auto fields = catalog->getFields(tableName);
        if (columns.empty() && values.size() > fields.size()) return false;
        for (const auto& column : columns) {
            if (!fieldExists(tableName, column)) return false;
        }

        std::map<std::string, std::string> valueMap = buildValueMap(tableName, columns, values);
        auto integrities = catalog->getIntegrities(tableName);

        for (const auto& integrity : integrities) {
            std::string field = safeString(integrity.field, Common::MAX_NAME_LEN);
            auto it = valueMap.find(field);
            std::string value = it == valueMap.end() ? "" : it->second;

            switch (integrity.type) {
                case PRIMARY_KEY:
                    if (!checkNotNull(value) || !checkPrimaryKey(tableName, field, value)) {
                        return false;
                    }
                    break;
                case UNIQUE:
                    if (!isNullValue(value) && !checkUnique(tableName, field, value)) {
                        return false;
                    }
                    break;
                case NOT_NULL:
                    if (!checkNotNull(value)) {
                        return false;
                    }
                    break;
                case CHECK_CONSTRAINT: {
                    std::string cond = safeString(integrity.param, Common::MAX_PATH_LEN);
                    if (!checkConstraint(cond, valueMap, fields)) {
                        std::cerr << "Error: CHECK 约束违反。" << std::endl;
                        return false;
                    }
                    break;
                }
                case FOREIGN_KEY: {
                    if (isNullValue(value)) break;

                    std::string param = safeString(integrity.param, Common::MAX_PATH_LEN);
                    size_t dot = param.find('.');
                    if (dot == std::string::npos || dot == 0 || dot == param.size() - 1) {
                        return false;
                    }

                    if (!checkForeignKey(value, param.substr(0, dot), param.substr(dot + 1))) {
                        return false;
                    }
                    break;
                }
                default:
                    break;
            }
        }

        return true;
    }

    bool IntegrityManagerImpl::checkUpdate(const std::string& tableName,
                                           const std::string& column,
                                           const std::string& value) {
        if (!catalog || !storage || !catalog->hasTable(tableName) || !fieldExists(tableName, column)) {
            return false;
        }

        auto integrities = catalog->getIntegrities(tableName);
        for (const auto& integrity : integrities) {
            std::string field = safeString(integrity.field, Common::MAX_NAME_LEN);
            if (field != column) continue;

            switch (integrity.type) {
                case PRIMARY_KEY:
                    if (!checkNotNull(value) || !checkPrimaryKey(tableName, column, value)) {
                        return false;
                    }
                    break;
                case UNIQUE:
                    if (!isNullValue(value) && !checkUnique(tableName, column, value)) {
                        return false;
                    }
                    break;
                case NOT_NULL:
                    if (!checkNotNull(value)) {
                        return false;
                    }
                    break;
                case CHECK_CONSTRAINT: {
                    auto fields = catalog->getFields(tableName);
                    std::map<std::string, std::string> vm; vm[field] = value;
                    std::string cond = safeString(integrity.param, Common::MAX_PATH_LEN);
                    if (!checkConstraint(cond, vm, fields)) {
                        std::cerr << "Error: CHECK 约束违反。" << std::endl;
                        return false;
                    }
                    break;
                }
                case FOREIGN_KEY: {
                    if (isNullValue(value)) break;

                    std::string param = safeString(integrity.param, Common::MAX_PATH_LEN);
                    size_t dot = param.find('.');
                    if (dot == std::string::npos || dot == 0 || dot == param.size() - 1) {
                        return false;
                    }

                    if (!checkForeignKey(value, param.substr(0, dot), param.substr(dot + 1))) {
                        return false;
                    }
                    break;
                }
                default:
                    break;
            }
        }

        return true;
    }

    bool IntegrityManagerImpl::checkDelete(const std::string& tableName,
                                           const std::string& pkValue) {
        if (!catalog || !storage || !catalog->hasTable(tableName) || isNullValue(pkValue)) {
            return false;
        }

        for (const auto& candidateTable : listCurrentDatabaseTables()) {
            auto integrities = catalog->getIntegrities(candidateTable);
            for (const auto& integrity : integrities) {
                if (integrity.type != FOREIGN_KEY) continue;

                std::string param = safeString(integrity.param, Common::MAX_PATH_LEN);
                size_t dot = param.find('.');
                if (dot == std::string::npos) continue;

                std::string referencedTable = param.substr(0, dot);
                if (referencedTable != tableName) continue;

                std::string foreignField = safeString(integrity.field, Common::MAX_NAME_LEN);
                auto rows = readRows(candidateTable);
                auto fields = catalog->getFields(candidateTable);
                sortFields(fields);

                for (const auto& row : rows) {
                    if (valuesEqual(getFieldValue(row, fields, foreignField), pkValue)) {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    bool IntegrityManagerImpl::checkNotNull(const std::string& value) const {
        return !isNullValue(value);
    }

    bool IntegrityManagerImpl::checkPrimaryKey(const std::string& tableName,
                                               const std::string& fieldName,
                                               const std::string& value) {
        return checkUnique(tableName, fieldName, value);
    }

    bool IntegrityManagerImpl::checkUnique(const std::string& tableName,
                                           const std::string& fieldName,
                                           const std::string& value) {
        auto rows = readRows(tableName);
        auto fields = catalog->getFields(tableName);
        sortFields(fields);

        for (const auto& row : rows) {
            if (valuesEqual(getFieldValue(row, fields, fieldName), value)) {
                return false;
            }
        }

        return true;
    }

    bool IntegrityManagerImpl::checkForeignKey(const std::string& fieldValue,
                                               const std::string& referencedTable,
                                               const std::string& referencedField) {
        if (!catalog->hasTable(referencedTable) || !fieldExists(referencedTable, referencedField)) {
            return false;
        }

        auto rows = readRows(referencedTable);
        auto fields = catalog->getFields(referencedTable);
        sortFields(fields);

        for (const auto& row : rows) {
            if (valuesEqual(getFieldValue(row, fields, referencedField), fieldValue)) {
                return true;
            }
        }

        return false;
    }

    std::map<std::string, std::string> IntegrityManagerImpl::buildValueMap(
        const std::string& tableName,
        const std::vector<std::string>& columns,
        const std::vector<std::string>& values) {

        std::map<std::string, std::string> result;
        auto fields = catalog->getFields(tableName);
        sortFields(fields);

        if (columns.empty()) {
            for (size_t i = 0; i < values.size() && i < fields.size(); ++i) {
                result[safeString(fields[i].name, Common::MAX_NAME_LEN)] = values[i];
            }
            return result;
        }

        for (size_t i = 0; i < columns.size() && i < values.size(); ++i) {
            result[columns[i]] = values[i];
        }

        return result;
    }

    bool IntegrityManagerImpl::fieldExists(const std::string& tableName,
                                           const std::string& fieldName) {
        auto fields = catalog->getFields(tableName);
        for (const auto& field : fields) {
            if (safeString(field.name, Common::MAX_NAME_LEN) == fieldName) {
                return true;
            }
        }
        return false;
    }

    std::vector<std::vector<std::string>> IntegrityManagerImpl::readRows(
        const std::string& tableName) {
        std::vector<std::vector<std::string>> rows;
        if (!catalog->hasTable(tableName)) return rows;

        Meta::TableBlock table = catalog->getTableMeta(tableName);
        std::string dbName = catalog->getCurrentDatabase();
        if (dbName.empty()) return rows;

        std::string filePath = "data/" + dbName + "/" + safeString(table.trd, Common::MAX_PATH_LEN);
        long fileSize = storage->getFileSize(filePath);
        if (fileSize <= 0 || fileSize > std::numeric_limits<int>::max()) return rows;

        std::vector<char> buffer(static_cast<size_t>(fileSize));
        if (!storage->readRaw(filePath, 0, static_cast<int>(fileSize), buffer.data())) {
            return rows;
        }

        if (looksLikeDelimitedText(buffer)) {
            std::string text(buffer.begin(), buffer.end());
            std::istringstream stream(text);
            std::string line;
            while (std::getline(stream, line)) {
                line = trim(line);
                if (!line.empty()) {
                    rows.push_back(splitDelimitedRecord(line));
                }
            }
            return rows;
        }

        auto fields = catalog->getFields(tableName);
        sortFields(fields);
        if (fields.empty()) return rows;

        int recordSize = 0;
        for (const auto& field : fields) {
            recordSize += fieldSize(field);
        }

        if (recordSize > 0 && fileSize % recordSize == 0) {
            int recordCount = static_cast<int>(fileSize / recordSize);
            for (int rowIndex = 0; rowIndex < recordCount; ++rowIndex) {
                std::vector<std::string> row;
                int offset = rowIndex * recordSize;
                for (const auto& field : fields) {
                    row.push_back(readBinaryValue(buffer.data() + offset, field));
                    offset += fieldSize(field);
                }
                rows.push_back(row);
            }
            return rows;
        }

        int fixed128Size = static_cast<int>(fields.size() * Common::MAX_NAME_LEN);
        int fixed256Size = static_cast<int>(fields.size() * Common::MAX_PATH_LEN);
        int fixedSize = 0;
        if (fixed128Size > 0 && fileSize % fixed128Size == 0) fixedSize = fixed128Size;
        else if (fixed256Size > 0 && fileSize % fixed256Size == 0) fixedSize = fixed256Size;

        if (fixedSize > 0) {
            int slotSize = fixedSize / static_cast<int>(fields.size());
            int recordCount = static_cast<int>(fileSize / fixedSize);
            for (int rowIndex = 0; rowIndex < recordCount; ++rowIndex) {
                std::vector<std::string> row;
                int offset = rowIndex * fixedSize;
                for (size_t fieldIndex = 0; fieldIndex < fields.size(); ++fieldIndex) {
                    row.push_back(normalizeValue(std::string(buffer.data() + offset, slotSize)));
                    offset += slotSize;
                }
                rows.push_back(row);
            }
        }

        return rows;
    }

    std::string IntegrityManagerImpl::getFieldValue(
        const std::vector<std::string>& row,
        const std::vector<Meta::FieldBlock>& fields,
        const std::string& fieldName) const {

        for (size_t i = 0; i < fields.size(); ++i) {
            if (safeString(fields[i].name, Common::MAX_NAME_LEN) == fieldName) {
                return i < row.size() ? row[i] : "";
            }
        }

        return "";
    }

    // CHECK 约束简单求值：替换列名为实际值，然后做数值或字符串比较
    bool IntegrityManagerImpl::checkConstraint(const std::string& condition,
                                                const std::map<std::string, std::string>& valueMap,
                                                const std::vector<Meta::FieldBlock>& fields) {
        std::string expr = condition;

        // 替换列名为实际值
        for (const auto& f : fields) {
            std::string name = safeString(f.name, Common::MAX_NAME_LEN);
            auto it = valueMap.find(name);
            if (it != valueMap.end()) {
                std::string val = it->second;
                // 去引号
                if (val.size() >= 2 && val.front() == '\'' && val.back() == '\'')
                    val = val.substr(1, val.size() - 2);
                // 单词级别的替换（防止 "age" 匹配 "age2"）
                size_t pos = 0;
                while ((pos = expr.find(name, pos)) != std::string::npos) {
                    bool leftOk = (pos == 0 || !std::isalnum(static_cast<unsigned char>(expr[pos - 1])));
                    bool rightOk = (pos + name.size() >= expr.size() || !std::isalnum(static_cast<unsigned char>(expr[pos + name.size()])));
                    if (leftOk && rightOk) {
                        expr.replace(pos, name.size(), val);
                        pos += val.size();
                    } else {
                        pos += name.size();
                    }
                }
            }
        }

        // 解析 AND/OR 连接的子条件
        // 先在顶层按 AND 拆，再按 OR 拆，递归求值
        // 查找顶层 AND（不在括号内的）
        std::function<bool(std::string)> evalExpr = [&](std::string s) -> bool {
            // 去除外层空格
            while (!s.empty() && s.front() == ' ') s.erase(0, 1);
            while (!s.empty() && s.back() == ' ') s.pop_back();

            // 查找顶层 AND
            int depth = 0;
            for (size_t i = 0; i < s.size(); ++i) {
                if (s[i] == '(') depth++;
                else if (s[i] == ')') depth--;
                else if (depth == 0 && i + 4 <= s.size() && s.substr(i, 4) == " AND") {
                    return evalExpr(s.substr(0, i)) && evalExpr(s.substr(i + 4));
                }
            }

            // 查找顶层 OR
            depth = 0;
            for (size_t i = 0; i < s.size(); ++i) {
                if (s[i] == '(') depth++;
                else if (s[i] == ')') depth--;
                else if (depth == 0 && i + 3 <= s.size() && s.substr(i, 3) == " OR") {
                    return evalExpr(s.substr(0, i)) || evalExpr(s.substr(i + 3));
                }
            }

            // 去除外层括号
            if (!s.empty() && s.front() == '(' && s.back() == ')')
                return evalExpr(s.substr(1, s.size() - 2));

            // 单个比较：左值 op 右值
            // 支持的运算符：>= <= <> != = > <
            std::string op;
            size_t opPos = std::string::npos;

            auto findOp = [&](const std::string& o) -> bool {
                size_t p = s.find(o);
                if (p != std::string::npos && p > 0 && p < s.size() - o.size()) {
                    op = o; opPos = p; return true;
                }
                return false;
            };

            if (!findOp(">=") && !findOp("<=") && !findOp("<>") && !findOp("!=")
                && !findOp("=") && !findOp(">") && !findOp("<"))
                return false;

            std::string lv = s.substr(0, opPos);
            std::string rv = s.substr(opPos + op.size());
            // 去空格
            while (!lv.empty() && lv.back() == ' ') lv.pop_back();
            while (!rv.empty() && rv.front() == ' ') rv.erase(0, 1);

            // 尝试数值比较
            try {
                double ln = std::stod(lv), rn = std::stod(rv);
                if (op == "=" || op == "==") return std::fabs(ln - rn) < 1e-9;
                if (op == "!=" || op == "<>") return std::fabs(ln - rn) >= 1e-9;
                if (op == ">") return ln > rn;
                if (op == "<") return ln < rn;
                if (op == ">=") return ln >= rn;
                if (op == "<=") return ln <= rn;
            } catch (...) {}
            // 字符串比较
            if (op == "=") return lv == rv;
            if (op == "!=" || op == "<>") return lv != rv;
            if (op == ">") return lv > rv;
            if (op == "<") return lv < rv;
            if (op == ">=") return lv >= rv;
            if (op == "<=") return lv <= rv;
            return false;
        };

        return evalExpr(expr);
    }

    std::vector<std::string> IntegrityManagerImpl::listCurrentDatabaseTables() {
        std::vector<std::string> tables;
        if (!catalog) return tables;

        std::string dbName = catalog->getCurrentDatabase();
        if (dbName.empty()) return tables;

        fs::path dbPath = fs::path("data") / dbName;
        std::error_code ec;
        if (!fs::exists(dbPath, ec) || !fs::is_directory(dbPath, ec)) {
            return tables;
        }

        for (const auto& entry : fs::directory_iterator(dbPath, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() == ".tb") {
                std::string tableName = entry.path().stem().string();
                if (catalog->hasTable(tableName)) {
                    tables.push_back(tableName);
                }
            }
        }

        return tables;
    }

}
