#ifndef INDEX_MAINTENANCE_H
#define INDEX_MAINTENANCE_H

#include "../../include/index/IIndexManager.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"

#include <string>
#include <vector>

namespace Execution {
namespace IndexMaintenance {

std::vector<Meta::FieldBlock> sortedFields(Catalog::ICatalogManager* catalog,
                                           const std::string& tableName);
int calcFieldSize(const Meta::FieldBlock& field);
int calcRecordSize(const std::vector<Meta::FieldBlock>& fields);
std::vector<int> calcFieldOffsets(const std::vector<Meta::FieldBlock>& fields);

std::string decodeValue(const char* data, const Meta::FieldBlock& field);
std::string encodeValueForIndex(const std::string& value, const Meta::FieldBlock& field);
std::vector<std::string> decodeRecord(const char* data,
                                      const std::vector<Meta::FieldBlock>& fields,
                                      const std::vector<int>& offsets);

bool buildIndexKey(const Meta::IndexBlock& indexMeta,
                   const std::vector<Meta::FieldBlock>& fields,
                   const std::vector<std::string>& row,
                   Index::IndexKey& key);

std::vector<Index::IndexRecord> readIndexRecords(const std::string& tableName,
                                                 const Meta::IndexBlock& indexMeta,
                                                 Catalog::ICatalogManager* catalog,
                                                 Storage::IStorageEngine* storage);

bool rebuildTableIndex(const std::string& tableName,
                       const Meta::IndexBlock& indexMeta,
                       Catalog::ICatalogManager* catalog,
                       Storage::IStorageEngine* storage,
                       Index::IIndexManager* indexManager);

bool rebuildTableIndexes(const std::string& tableName,
                         Catalog::ICatalogManager* catalog,
                         Storage::IStorageEngine* storage,
                         Index::IIndexManager* indexManager);

bool canRebuildTableIndexesFromRows(const std::string& tableName,
                                    Catalog::ICatalogManager* catalog,
                                    Storage::IStorageEngine* storage,
                                    Index::IIndexManager* indexManager,
                                    const std::vector<std::vector<std::string>>& rows);

bool validateUniqueIndexes(const std::string& tableName,
                           Catalog::ICatalogManager* catalog,
                           Index::IIndexManager* indexManager,
                           const std::vector<std::string>& row);

bool insertRowIndexes(const std::string& tableName,
                      Catalog::ICatalogManager* catalog,
                      Index::IIndexManager* indexManager,
                      const std::vector<std::string>& row,
                      long recordOffset);

} // namespace IndexMaintenance
} // namespace Execution

#endif // INDEX_MAINTENANCE_H
