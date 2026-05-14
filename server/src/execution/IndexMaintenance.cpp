#include "IndexMaintenance.h"

#include "../../include/common/Constants.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <windows.h>

namespace Execution {
namespace IndexMaintenance {

namespace {

size_t boundedLength(const char* data, size_t maxLen) {
    size_t len = 0;
    while (len < maxLen && data[len] != '\0') ++len;
    return len;
}

int fieldIndexByName(const std::vector<Meta::FieldBlock>& fields,
                     const std::string& name) {
    for (size_t i = 0; i < fields.size(); ++i) {
        if (std::string(fields[i].name) == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string bytesFromUint64(uint64_t value) {
    std::string out(8, '\0');
    for (int i = 7; i >= 0; --i) {
        out[7 - i] = static_cast<char>((value >> (i * 8)) & 0xFF);
    }
    return out;
}

std::string encodeForIndex(const std::string& value, const Meta::FieldBlock& field) {
    if (field.type == static_cast<int>(Common::DataType::INTEGER) ||
        field.type == static_cast<int>(Common::DataType::BOOL)) {
        int parsed = 0;
        if (field.type == static_cast<int>(Common::DataType::BOOL)) {
            std::string upper = value;
            std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
            parsed = (upper == "TRUE" || upper == "1") ? 1 : 0;
        } else {
            try { parsed = std::stoi(value); } catch (...) {}
        }
        uint32_t sortable = static_cast<uint32_t>(parsed) ^ 0x80000000u;
        std::string out(4, '\0');
        out[0] = static_cast<char>((sortable >> 24) & 0xFF);
        out[1] = static_cast<char>((sortable >> 16) & 0xFF);
        out[2] = static_cast<char>((sortable >> 8) & 0xFF);
        out[3] = static_cast<char>(sortable & 0xFF);
        return out;
    }

    if (field.type == static_cast<int>(Common::DataType::DOUBLE)) {
        double parsed = 0;
        try { parsed = std::stod(value); } catch (...) {}
        uint64_t bits = 0;
        std::memcpy(&bits, &parsed, sizeof(bits));
        uint64_t sortable = (bits & (1ULL << 63)) ? ~bits : (bits ^ (1ULL << 63));
        return bytesFromUint64(sortable);
    }

    if (field.type == static_cast<int>(Common::DataType::DATETIME)) {
        int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
        std::sscanf(value.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);
        uint64_t packed = static_cast<uint64_t>(year) * 10000000000ULL +
                          static_cast<uint64_t>(month) * 100000000ULL +
                          static_cast<uint64_t>(day) * 1000000ULL +
                          static_cast<uint64_t>(hour) * 10000ULL +
                          static_cast<uint64_t>(minute) * 100ULL +
                          static_cast<uint64_t>(second);
        return bytesFromUint64(packed);
    }

    return value;
}

} // namespace

std::vector<Meta::FieldBlock> sortedFields(Catalog::ICatalogManager* catalog,
                                           const std::string& tableName) {
    std::vector<Meta::FieldBlock> fields;
    if (!catalog) return fields;
    fields = catalog->getFields(tableName);
    std::sort(fields.begin(), fields.end(),
              [](const Meta::FieldBlock& a, const Meta::FieldBlock& b) {
                  return a.order < b.order;
              });
    return fields;
}

int calcFieldSize(const Meta::FieldBlock& field) {
    if (field.type == static_cast<int>(Common::DataType::INTEGER) ||
        field.type == static_cast<int>(Common::DataType::BOOL)) {
        return 4;
    }
    if (field.type == static_cast<int>(Common::DataType::DOUBLE)) {
        return 8;
    }
    if (field.type == static_cast<int>(Common::DataType::VARCHAR)) {
        int n = field.param > 0 ? field.param : Common::MAX_PATH_LEN;
        return ((n + 3) / 4) * 4;
    }
    if (field.type == static_cast<int>(Common::DataType::DATETIME)) {
        return ((sizeof(SYSTEMTIME) + 3) / 4) * 4;
    }
    return 4;
}

int calcRecordSize(const std::vector<Meta::FieldBlock>& fields) {
    int size = 0;
    for (const auto& field : fields) {
        size += calcFieldSize(field);
    }
    return size;
}

std::vector<int> calcFieldOffsets(const std::vector<Meta::FieldBlock>& fields) {
    std::vector<int> offsets;
    int offset = 0;
    for (const auto& field : fields) {
        offsets.push_back(offset);
        offset += calcFieldSize(field);
    }
    return offsets;
}

std::string decodeValue(const char* data, const Meta::FieldBlock& field) {
    if (!data) return "";

    if (field.type == static_cast<int>(Common::DataType::INTEGER)) {
        int value = 0;
        std::memcpy(&value, data, sizeof(int));
        return std::to_string(value);
    }
    if (field.type == static_cast<int>(Common::DataType::BOOL)) {
        int value = 0;
        std::memcpy(&value, data, sizeof(int));
        return value ? "TRUE" : "FALSE";
    }
    if (field.type == static_cast<int>(Common::DataType::DOUBLE)) {
        double value = 0;
        std::memcpy(&value, data, sizeof(double));
        return std::to_string(value);
    }
    if (field.type == static_cast<int>(Common::DataType::VARCHAR)) {
        int n = field.param > 0 ? field.param : Common::MAX_PATH_LEN;
        return std::string(data, boundedLength(data, static_cast<size_t>(n)));
    }
    if (field.type == static_cast<int>(Common::DataType::DATETIME)) {
        SYSTEMTIME st;
        std::memset(&st, 0, sizeof(st));
        std::memcpy(&st, data, sizeof(SYSTEMTIME));
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                      st.wYear, st.wMonth, st.wDay,
                      st.wHour, st.wMinute, st.wSecond);
        return buffer;
    }
    return "";
}

std::string encodeValueForIndex(const std::string& value, const Meta::FieldBlock& field) {
    return encodeForIndex(value, field);
}

std::vector<std::string> decodeRecord(const char* data,
                                      const std::vector<Meta::FieldBlock>& fields,
                                      const std::vector<int>& offsets) {
    std::vector<std::string> row;
    if (!data || fields.size() != offsets.size()) return row;

    row.reserve(fields.size());
    for (size_t i = 0; i < fields.size(); ++i) {
        row.push_back(decodeValue(data + offsets[i], fields[i]));
    }
    return row;
}

bool buildIndexKey(const Meta::IndexBlock& indexMeta,
                   const std::vector<Meta::FieldBlock>& fields,
                   const std::vector<std::string>& row,
                   Index::IndexKey& key) {
    if (indexMeta.field_num <= 0 || indexMeta.field_num > 2) return false;

    key.values.clear();
    for (int i = 0; i < indexMeta.field_num; ++i) {
        int fieldIndex = fieldIndexByName(fields, indexMeta.fields[i]);
        if (fieldIndex < 0 || fieldIndex >= static_cast<int>(row.size())) {
            return false;
        }
        key.values.push_back(encodeForIndex(row[fieldIndex], fields[fieldIndex]));
    }
    return true;
}

std::vector<Index::IndexRecord> readIndexRecords(const std::string& tableName,
                                                 const Meta::IndexBlock& indexMeta,
                                                 Catalog::ICatalogManager* catalog,
                                                 Storage::IStorageEngine* storage) {
    std::vector<Index::IndexRecord> records;
    if (!catalog || !storage || tableName.empty()) return records;

    auto fields = sortedFields(catalog, tableName);
    auto offsets = calcFieldOffsets(fields);
    int recordSize = calcRecordSize(fields);
    if (recordSize <= 0) return records;

    std::string recordFile = indexMeta.record_file;
    if (recordFile.empty()) {
        Meta::TableBlock tableMeta = catalog->getTableMeta(tableName);
        recordFile = "data/" + catalog->getCurrentDatabase() + "/" + std::string(tableMeta.trd);
    }

    long fileSize = storage->getFileSize(recordFile);
    if (fileSize <= 0) return records;

    std::vector<char> buffer(static_cast<size_t>(fileSize));
    if (!storage->readRaw(recordFile, 0, static_cast<int>(fileSize), buffer.data())) {
        return records;
    }

    int rowCount = static_cast<int>(fileSize / recordSize);
    records.reserve(rowCount);
    for (int i = 0; i < rowCount; ++i) {
        const long recordOffset = static_cast<long>(i) * recordSize;
        auto row = decodeRecord(buffer.data() + recordOffset, fields, offsets);
        Index::IndexKey key;
        if (buildIndexKey(indexMeta, fields, row, key)) {
            records.push_back(Index::IndexRecord{key, recordOffset});
        }
    }
    return records;
}

bool rebuildTableIndex(const std::string& tableName,
                       const Meta::IndexBlock& indexMeta,
                       Catalog::ICatalogManager* catalog,
                       Storage::IStorageEngine* storage,
                       Index::IIndexManager* indexManager) {
    if (!indexManager) return true;
    auto records = readIndexRecords(tableName, indexMeta, catalog, storage);
    return indexManager->rebuildIndex(indexMeta, records);
}

bool rebuildTableIndexes(const std::string& tableName,
                         Catalog::ICatalogManager* catalog,
                         Storage::IStorageEngine* storage,
                         Index::IIndexManager* indexManager) {
    if (!catalog || !storage || !indexManager) return true;
    auto indices = indexManager->getIndices(catalog->getCurrentDatabase(), tableName);
    for (const auto& indexMeta : indices) {
        if (!rebuildTableIndex(tableName, indexMeta, catalog, storage, indexManager)) {
            return false;
        }
    }
    return true;
}

bool canRebuildTableIndexesFromRows(const std::string& tableName,
                                    Catalog::ICatalogManager* catalog,
                                    Storage::IStorageEngine* storage,
                                    Index::IIndexManager* indexManager,
                                    const std::vector<std::vector<std::string>>& rows) {
    if (!catalog || !storage || !indexManager) return true;

    auto fields = sortedFields(catalog, tableName);
    auto indices = indexManager->getIndices(catalog->getCurrentDatabase(), tableName);
    for (const auto& indexMeta : indices) {
        std::set<std::vector<std::string>> uniqueKeys;
        std::vector<Index::IndexRecord> records;
        records.reserve(rows.size());

        for (size_t i = 0; i < rows.size(); ++i) {
            Index::IndexKey key;
            if (!buildIndexKey(indexMeta, fields, rows[i], key)) {
                return false;
            }
            if (indexMeta.unique && !uniqueKeys.insert(key.values).second) {
                return false;
            }
            records.push_back(Index::IndexRecord{
                key,
                static_cast<long>(i) * calcRecordSize(fields)
            });
        }
    }
    return true;
}

bool validateUniqueIndexes(const std::string& tableName,
                           Catalog::ICatalogManager* catalog,
                           Index::IIndexManager* indexManager,
                           const std::vector<std::string>& row) {
    if (!catalog || !indexManager) return true;

    auto fields = sortedFields(catalog, tableName);
    auto indices = indexManager->getIndices(catalog->getCurrentDatabase(), tableName);
    for (const auto& indexMeta : indices) {
        if (!indexMeta.unique) continue;

        Index::IndexKey key;
        if (!buildIndexKey(indexMeta, fields, row, key)) {
            return false;
        }
        if (!indexManager->search(indexMeta, key).empty()) {
            return false;
        }
    }
    return true;
}

bool insertRowIndexes(const std::string& tableName,
                      Catalog::ICatalogManager* catalog,
                      Index::IIndexManager* indexManager,
                      const std::vector<std::string>& row,
                      long recordOffset) {
    if (!catalog || !indexManager) return true;

    auto fields = sortedFields(catalog, tableName);
    auto indices = indexManager->getIndices(catalog->getCurrentDatabase(), tableName);
    for (const auto& indexMeta : indices) {
        Index::IndexKey key;
        if (!buildIndexKey(indexMeta, fields, row, key)) {
            return false;
        }
        if (!indexManager->insertEntry(indexMeta, key, recordOffset)) {
            return false;
        }
    }
    return true;
}

} // namespace IndexMaintenance
} // namespace Execution
