#ifndef INDEX_MANAGER_IMPL_H
#define INDEX_MANAGER_IMPL_H

#include "../../include/index/IIndexManager.h"
#include "../../include/storage/IStorageEngine.h"

#include <string>
#include <vector>

namespace Index {

    class IndexManagerImpl : public IIndexManager {
    public:
        explicit IndexManagerImpl(Storage::IStorageEngine* storage);
        ~IndexManagerImpl() override = default;

        bool createIndex(const IndexCreateOptions& options) override;
        bool dropIndex(const std::string& databaseName,
                       const std::string& tableName,
                       const std::string& indexName) override;

        std::vector<Meta::IndexBlock> getIndices(const std::string& databaseName,
                                                 const std::string& tableName) override;
        bool hasIndex(const std::string& databaseName,
                      const std::string& tableName,
                      const std::string& indexName) override;

        bool insertEntry(const Meta::IndexBlock& indexMeta,
                         const IndexKey& key,
                         long recordOffset) override;
        bool deleteEntry(const Meta::IndexBlock& indexMeta,
                         const IndexKey& key,
                         long recordOffset) override;
        std::vector<long> search(const Meta::IndexBlock& indexMeta,
                                 const IndexKey& key) override;
        std::vector<long> rangeSearch(const Meta::IndexBlock& indexMeta,
                                      const IndexKey& lower,
                                      const IndexKey& upper,
                                      bool includeLower = true,
                                      bool includeUpper = true) override;

        bool rebuildIndex(const Meta::IndexBlock& indexMeta,
                          const std::vector<IndexRecord>& records) override;

    private:
        Storage::IStorageEngine* storage_;

        static constexpr int kMaxFields = 2;
        static constexpr int kMaxKeyBytes = 256;
        static constexpr int kNodeOrder = 32;
        static constexpr int kInvalidPage = -1;

        struct FileHeader;
        struct TreeNode;
        struct KeyEntry;

        std::string databaseDir(const std::string& databaseName) const;
        std::string tableTidPath(const std::string& databaseName, const std::string& tableName) const;
        std::string makeIndexFilePath(const std::string& databaseName,
                                      const std::string& tableName,
                                      const std::string& indexName) const;

        bool writeIndexCatalog(const std::string& databaseName,
                               const std::string& tableName,
                               const std::vector<Meta::IndexBlock>& indices);
        bool appendIndexCatalog(const std::string& databaseName,
                                const std::string& tableName,
                                const Meta::IndexBlock& indexMeta);

        bool initializeIndexFile(const std::string& indexFile);
        bool loadHeader(const std::string& indexFile, FileHeader& header);
        bool saveHeader(const std::string& indexFile, const FileHeader& header);
        bool readNode(const std::string& indexFile, int pageId, TreeNode& node);
        bool writeNode(const std::string& indexFile, const TreeNode& node);
        bool allocateNode(const std::string& indexFile, FileHeader& header, bool leaf, TreeNode& node);

        KeyEntry makeEntry(const IndexKey& key, long recordOffset) const;
        int compareKey(const KeyEntry& lhs, const KeyEntry& rhs) const;
        int compareSearchKey(const KeyEntry& lhs, const IndexKey& rhs) const;
        bool entryEquals(const KeyEntry& lhs, const IndexKey& key, long recordOffset) const;
        bool keyExists(const std::string& indexFile, const IndexKey& key);

        bool insertEntryInternal(const std::string& indexFile,
                                 const IndexKey& key,
                                 const KeyEntry& entry,
                                 bool unique);
        bool insertIntoLeaf(const std::string& indexFile,
                            FileHeader& header,
                            TreeNode& leaf,
                            const KeyEntry& entry,
                            bool unique,
                            KeyEntry& promotedKey,
                            int& promotedPage);
        bool insertIntoParent(const std::string& indexFile,
                              FileHeader& header,
                              TreeNode& left,
                              const KeyEntry& key,
                              TreeNode& right);
        bool splitInternalNode(const std::string& indexFile,
                               FileHeader& header,
                               TreeNode& node,
                               KeyEntry& promotedKey,
                               int& promotedPage);
        int findLeafPage(const std::string& indexFile,
                         const FileHeader& header,
                         const IndexKey& key);
        int findLeftmostLeafPage(const std::string& indexFile,
                                 const FileHeader& header,
                                 const IndexKey& key);
    };

}

#endif // INDEX_MANAGER_IMPL_H
