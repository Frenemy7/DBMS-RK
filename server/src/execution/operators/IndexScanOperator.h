#ifndef INDEX_SCAN_OPERATOR_H
#define INDEX_SCAN_OPERATOR_H

#include "../../../include/execution/IOperator.h"
#include "../../../include/catalog/ICatalogManager.h"
#include "../../../include/storage/IStorageEngine.h"
#include "../../../include/index/IIndexManager.h"

#include <string>
#include <vector>

namespace Execution {

class IndexScanOperator : public IOperator {
private:
    std::string tableName_;
    Meta::IndexBlock indexMeta_;
    Index::IndexKey key_;
    Index::IndexKey upperKey_;
    bool rangeMode_;
    bool includeLower_;
    bool includeUpper_;
    Catalog::ICatalogManager* catalog_;
    Storage::IStorageEngine* storage_;
    Index::IIndexManager* indexManager_;

    std::vector<Meta::FieldBlock> fields_;
    std::vector<int> offsets_;
    std::vector<long> recordOffsets_;
    std::string recordFile_;
    int recordSize_;
    size_t cursor_;

public:
    IndexScanOperator(std::string tableName,
                      const Meta::IndexBlock& indexMeta,
                      Index::IndexKey key,
                      Catalog::ICatalogManager* catalog,
                      Storage::IStorageEngine* storage,
                      Index::IIndexManager* indexManager);

    IndexScanOperator(std::string tableName,
                      const Meta::IndexBlock& indexMeta,
                      Index::IndexKey lowerKey,
                      Index::IndexKey upperKey,
                      bool includeLower,
                      bool includeUpper,
                      Catalog::ICatalogManager* catalog,
                      Storage::IStorageEngine* storage,
                      Index::IIndexManager* indexManager);

    bool init() override;
    bool next(std::vector<std::string>& row) override;
    std::vector<Meta::FieldBlock> getOutputSchema() const override;
};

} // namespace Execution

#endif // INDEX_SCAN_OPERATOR_H
