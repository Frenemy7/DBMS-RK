#ifndef SEQ_SCAN_OPERATOR_H
#define SEQ_SCAN_OPERATOR_H

#include "../../../include/execution/IOperator.h"
#include "../../../include/catalog/ICatalogManager.h"
#include "../../../include/storage/IStorageEngine.h"

namespace Execution {

    class SeqScanOperator : public IOperator {
    private:
        std::string tableName;
        Catalog::ICatalogManager* catalog;
        Storage::IStorageEngine* storage;

        // 状态记录：记录当前读到了文件的哪个位置
        long currentOffset; 
        long fileSize;
        std::vector<Meta::FieldBlock> fields;
        int recordSize; // 单行记录占用的物理字节数

        char* buffer; // 用于一次性把文件读进内存的缓存

    public:
        SeqScanOperator(std::string tableName, Catalog::ICatalogManager* catalog, Storage::IStorageEngine* storage);
        ~SeqScanOperator() override;

        bool init() override;
        bool next(std::vector<std::string>& row) override;
        std::vector<Meta::FieldBlock> getOutputSchema() const override;
    };

}
#endif