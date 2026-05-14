#include "IndexScanOperator.h"
#include "../IndexMaintenance.h"

#include <utility>

namespace Execution {

IndexScanOperator::IndexScanOperator(std::string tableName,
                                     const Meta::IndexBlock& indexMeta,
                                     Index::IndexKey key,
                                     Catalog::ICatalogManager* catalog,
                                     Storage::IStorageEngine* storage,
                                     Index::IIndexManager* indexManager)
    : tableName_(std::move(tableName)),
      indexMeta_(indexMeta),
      key_(std::move(key)),
      rangeMode_(false),
      includeLower_(true),
      includeUpper_(true),
      catalog_(catalog),
      storage_(storage),
      indexManager_(indexManager),
      recordSize_(0),
      cursor_(0) {}

IndexScanOperator::IndexScanOperator(std::string tableName,
                                     const Meta::IndexBlock& indexMeta,
                                     Index::IndexKey lowerKey,
                                     Index::IndexKey upperKey,
                                     bool includeLower,
                                     bool includeUpper,
                                     Catalog::ICatalogManager* catalog,
                                     Storage::IStorageEngine* storage,
                                     Index::IIndexManager* indexManager)
    : tableName_(std::move(tableName)),
      indexMeta_(indexMeta),
      key_(std::move(lowerKey)),
      upperKey_(std::move(upperKey)),
      rangeMode_(true),
      includeLower_(includeLower),
      includeUpper_(includeUpper),
      catalog_(catalog),
      storage_(storage),
      indexManager_(indexManager),
      recordSize_(0),
      cursor_(0) {}

bool IndexScanOperator::init() {
    if (!catalog_ || !storage_ || !indexManager_ || !catalog_->hasTable(tableName_)) {
        return false;
    }

    fields_ = IndexMaintenance::sortedFields(catalog_, tableName_);
    offsets_ = IndexMaintenance::calcFieldOffsets(fields_);
    recordSize_ = IndexMaintenance::calcRecordSize(fields_);
    if (recordSize_ <= 0) return false;

    recordFile_ = indexMeta_.record_file;
    if (recordFile_.empty()) {
        Meta::TableBlock tableMeta = catalog_->getTableMeta(tableName_);
        recordFile_ = "data/" + catalog_->getCurrentDatabase() + "/" + std::string(tableMeta.trd);
    }

    recordOffsets_ = rangeMode_
        ? indexManager_->rangeSearch(indexMeta_, key_, upperKey_, includeLower_, includeUpper_)
        : indexManager_->search(indexMeta_, key_);
    cursor_ = 0;
    return true;
}

bool IndexScanOperator::next(std::vector<std::string>& row) {
    if (recordSize_ <= 0) return false;

    while (cursor_ < recordOffsets_.size()) {
        long offset = recordOffsets_[cursor_++];
        if (offset < 0) continue;

        std::vector<char> buffer(static_cast<size_t>(recordSize_));
        if (!storage_->readRaw(recordFile_, offset, recordSize_, buffer.data())) {
            continue;
        }

        row = IndexMaintenance::decodeRecord(buffer.data(), fields_, offsets_);
        return true;
    }
    return false;
}

std::vector<Meta::FieldBlock> IndexScanOperator::getOutputSchema() const {
    return fields_;
}

} // namespace Execution
