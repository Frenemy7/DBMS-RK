#ifndef IINDEX_MANAGER_H
#define IINDEX_MANAGER_H

#include "../meta/TableMeta.h"

#include <string>
#include <utility>
#include <vector>

namespace Index {

    struct IndexKey {
        std::vector<std::string> values;

        IndexKey() = default;
        explicit IndexKey(std::vector<std::string> keyValues) : values(std::move(keyValues)) {}
    };

    struct IndexRecord {
        IndexKey key;
        long recordOffset = -1;
    };

    struct IndexCreateOptions {
        std::string databaseName;
        std::string tableName;
        std::string indexName;
        std::vector<std::string> fields;
        bool unique = false;
        bool asc = true;
        std::string recordFile;
    };

    class IIndexManager {
    public:
        virtual ~IIndexManager() = default;

        virtual bool createIndex(const IndexCreateOptions& options) = 0;
        virtual bool dropIndex(const std::string& databaseName,
                               const std::string& tableName,
                               const std::string& indexName) = 0;

        virtual std::vector<Meta::IndexBlock> getIndices(const std::string& databaseName,
                                                         const std::string& tableName) = 0;
        virtual bool hasIndex(const std::string& databaseName,
                              const std::string& tableName,
                              const std::string& indexName) = 0;

        virtual bool insertEntry(const Meta::IndexBlock& indexMeta,
                                 const IndexKey& key,
                                 long recordOffset) = 0;
        virtual bool deleteEntry(const Meta::IndexBlock& indexMeta,
                                 const IndexKey& key,
                                 long recordOffset) = 0;
        virtual std::vector<long> search(const Meta::IndexBlock& indexMeta,
                                         const IndexKey& key) = 0;
        virtual std::vector<long> rangeSearch(const Meta::IndexBlock& indexMeta,
                                              const IndexKey& lower,
                                              const IndexKey& upper,
                                              bool includeLower = true,
                                              bool includeUpper = true) = 0;

        virtual bool rebuildIndex(const Meta::IndexBlock& indexMeta,
                                  const std::vector<IndexRecord>& records) = 0;
    };

}

#endif // IINDEX_MANAGER_H
