#include "IndexManagerImpl.h"

#include "../../include/common/Constants.h"

#include <algorithm>
#include <cstring>

namespace Index {

#pragma pack(push, 4)
    struct IndexManagerImpl::FileHeader {
        char magic[8];
        int version;
        int rootPage;
        int nextPage;
        int nodeOrder;
        int keyBytes;
    };

    struct IndexManagerImpl::KeyEntry {
        char key[kMaxKeyBytes];
        long recordOffset;
    };

    struct IndexManagerImpl::TreeNode {
        int pageId;
        bool isLeaf;
        int keyCount;
        int parentPage;
        int prevLeaf;
        int nextLeaf;
        KeyEntry entries[kNodeOrder + 1];
        int children[kNodeOrder + 2];
    };
#pragma pack(pop)

    namespace {
        constexpr const char* kMagic = "RKIX001";

        void clearIndexBlock(Meta::IndexBlock& block) {
            std::memset(&block, 0, sizeof(Meta::IndexBlock));
        }

        void copyCString(char* dest, size_t destSize, const std::string& value) {
            if (destSize == 0) return;
            std::memset(dest, 0, destSize);
            std::strncpy(dest, value.c_str(), destSize - 1);
        }
    }

    IndexManagerImpl::IndexManagerImpl(Storage::IStorageEngine* storage)
        : storage_(storage) {}

    bool IndexManagerImpl::createIndex(const IndexCreateOptions& options) {
        if (!storage_) return false;
        if (options.databaseName.empty() || options.tableName.empty() ||
            options.indexName.empty() || options.fields.empty() ||
            options.fields.size() > kMaxFields) {
            return false;
        }
        if (hasIndex(options.databaseName, options.tableName, options.indexName)) {
            return false;
        }

        const std::string indexFile = makeIndexFilePath(
            options.databaseName, options.tableName, options.indexName);
        const std::string tidFile = tableTidPath(options.databaseName, options.tableName);

        if (storage_->getFileSize(tidFile) < 0) {
            return false;
        }
        if (storage_->getFileSize(indexFile) >= 0) {
            return false;
        }
        if (!initializeIndexFile(indexFile)) {
            return false;
        }

        Meta::IndexBlock indexMeta;
        clearIndexBlock(indexMeta);
        copyCString(indexMeta.name, sizeof(indexMeta.name), options.indexName);
        indexMeta.unique = options.unique;
        indexMeta.asc = options.asc;
        indexMeta.field_num = static_cast<int>(options.fields.size());
        for (size_t i = 0; i < options.fields.size(); ++i) {
            copyCString(indexMeta.fields[i], sizeof(indexMeta.fields[i]), options.fields[i]);
        }

        const std::string recordFile = options.recordFile.empty()
            ? databaseDir(options.databaseName) + "/" + options.tableName + ".trd"
            : options.recordFile;

        copyCString(indexMeta.record_file, sizeof(indexMeta.record_file), recordFile);
        copyCString(indexMeta.index_file, sizeof(indexMeta.index_file), indexFile);

        return appendIndexCatalog(options.databaseName, options.tableName, indexMeta);
    }

    bool IndexManagerImpl::dropIndex(const std::string& databaseName,
                                     const std::string& tableName,
                                     const std::string& indexName) {
        if (!storage_) return false;

        auto indices = getIndices(databaseName, tableName);
        std::vector<Meta::IndexBlock> kept;
        std::string indexFile;
        bool found = false;

        for (const auto& index : indices) {
            if (std::string(index.name) == indexName) {
                found = true;
                indexFile = index.index_file;
            } else {
                kept.push_back(index);
            }
        }

        if (!found) return false;
        if (!writeIndexCatalog(databaseName, tableName, kept)) return false;

        if (!indexFile.empty() && storage_->getFileSize(indexFile) >= 0) {
            storage_->deleteFile(indexFile);
        }
        return true;
    }

    std::vector<Meta::IndexBlock> IndexManagerImpl::getIndices(const std::string& databaseName,
                                                               const std::string& tableName) {
        std::vector<Meta::IndexBlock> indices;
        if (!storage_) return indices;

        const std::string tidFile = tableTidPath(databaseName, tableName);
        const long fileSize = storage_->getFileSize(tidFile);
        if (fileSize <= 0) return indices;
        if (fileSize % static_cast<long>(sizeof(Meta::IndexBlock)) != 0) return indices;

        std::vector<char> buffer(static_cast<size_t>(fileSize));
        if (!storage_->readRaw(tidFile, 0, static_cast<int>(fileSize), buffer.data())) {
            return indices;
        }

        const int count = static_cast<int>(fileSize / sizeof(Meta::IndexBlock));
        for (int i = 0; i < count; ++i) {
            Meta::IndexBlock block;
            std::memcpy(&block, buffer.data() + i * sizeof(Meta::IndexBlock), sizeof(Meta::IndexBlock));
            indices.push_back(block);
        }
        return indices;
    }

    bool IndexManagerImpl::hasIndex(const std::string& databaseName,
                                    const std::string& tableName,
                                    const std::string& indexName) {
        auto indices = getIndices(databaseName, tableName);
        return std::any_of(indices.begin(), indices.end(), [&](const Meta::IndexBlock& index) {
            return std::string(index.name) == indexName;
        });
    }

    bool IndexManagerImpl::insertEntry(const Meta::IndexBlock& indexMeta,
                                       const IndexKey& key,
                                       long recordOffset) {
        if (!storage_ || recordOffset < 0) return false;
        return insertEntryInternal(indexMeta.index_file, key, makeEntry(key, recordOffset), indexMeta.unique);
    }

    bool IndexManagerImpl::deleteEntry(const Meta::IndexBlock& indexMeta,
                                       const IndexKey& key,
                                       long recordOffset) {
        if (!storage_) return false;

        FileHeader header;
        if (!loadHeader(indexMeta.index_file, header) || header.rootPage == kInvalidPage) return false;

        int leafPage = findLeafPage(indexMeta.index_file, header, key);
        if (leafPage == kInvalidPage) return false;

        TreeNode leaf;
        if (!readNode(indexMeta.index_file, leafPage, leaf) || !leaf.isLeaf) return false;

        for (int i = 0; i < leaf.keyCount; ++i) {
            if (entryEquals(leaf.entries[i], key, recordOffset)) {
                for (int j = i; j < leaf.keyCount - 1; ++j) {
                    leaf.entries[j] = leaf.entries[j + 1];
                }
                --leaf.keyCount;
                return writeNode(indexMeta.index_file, leaf);
            }
        }
        return false;
    }

    std::vector<long> IndexManagerImpl::search(const Meta::IndexBlock& indexMeta,
                                               const IndexKey& key) {
        std::vector<long> offsets;
        if (!storage_) return offsets;

        FileHeader header;
        if (!loadHeader(indexMeta.index_file, header) || header.rootPage == kInvalidPage) return offsets;

        int leafPage = findLeftmostLeafPage(indexMeta.index_file, header, key);
        while (leafPage != kInvalidPage) {
            TreeNode leaf;
            if (!readNode(indexMeta.index_file, leafPage, leaf)) break;

            bool sawGreater = false;
            for (int i = 0; i < leaf.keyCount; ++i) {
                int cmp = compareSearchKey(leaf.entries[i], key);
                if (cmp == 0) {
                    offsets.push_back(leaf.entries[i].recordOffset);
                } else if (cmp > 0) {
                    sawGreater = true;
                    break;
                }
            }
            if (sawGreater) break;
            leafPage = leaf.nextLeaf;
        }
        return offsets;
    }

    std::vector<long> IndexManagerImpl::rangeSearch(const Meta::IndexBlock& indexMeta,
                                                    const IndexKey& lower,
                                                    const IndexKey& upper,
                                                    bool includeLower,
                                                    bool includeUpper) {
        std::vector<long> offsets;
        if (!storage_) return offsets;

        FileHeader header;
        if (!loadHeader(indexMeta.index_file, header) || header.rootPage == kInvalidPage) return offsets;

        int leafPage = findLeftmostLeafPage(indexMeta.index_file, header, lower);
        while (leafPage != kInvalidPage) {
            TreeNode leaf;
            if (!readNode(indexMeta.index_file, leafPage, leaf)) break;

            for (int i = 0; i < leaf.keyCount; ++i) {
                const int lowerCmp = compareSearchKey(leaf.entries[i], lower);
                const int upperCmp = compareSearchKey(leaf.entries[i], upper);
                const bool passLower = includeLower ? lowerCmp >= 0 : lowerCmp > 0;
                const bool passUpper = includeUpper ? upperCmp <= 0 : upperCmp < 0;

                if (passLower && passUpper) {
                    offsets.push_back(leaf.entries[i].recordOffset);
                }
                if (!passUpper) {
                    return offsets;
                }
            }
            leafPage = leaf.nextLeaf;
        }
        return offsets;
    }

    bool IndexManagerImpl::rebuildIndex(const Meta::IndexBlock& indexMeta,
                                        const std::vector<IndexRecord>& records) {
        if (!storage_) return false;

        const std::string indexFile = indexMeta.index_file;
        if (storage_->getFileSize(indexFile) >= 0 && !storage_->deleteFile(indexFile)) {
            return false;
        }
        if (!initializeIndexFile(indexFile)) {
            return false;
        }

        for (const auto& record : records) {
            if (!insertEntryInternal(indexFile, record.key, makeEntry(record.key, record.recordOffset), indexMeta.unique)) {
                return false;
            }
        }
        return true;
    }

    std::string IndexManagerImpl::databaseDir(const std::string& databaseName) const {
        return std::string(Common::DBMS_ROOT) + databaseName;
    }

    std::string IndexManagerImpl::tableTidPath(const std::string& databaseName,
                                               const std::string& tableName) const {
        return databaseDir(databaseName) + "/" + tableName + ".tid";
    }

    std::string IndexManagerImpl::makeIndexFilePath(const std::string& databaseName,
                                                    const std::string& tableName,
                                                    const std::string& indexName) const {
        return databaseDir(databaseName) + "/" + tableName + "_" + indexName + ".ix";
    }

    bool IndexManagerImpl::writeIndexCatalog(const std::string& databaseName,
                                             const std::string& tableName,
                                             const std::vector<Meta::IndexBlock>& indices) {
        const std::string tidFile = tableTidPath(databaseName, tableName);
        if (!storage_->clearFile(tidFile)) return false;

        for (const auto& index : indices) {
            if (storage_->appendRaw(tidFile, sizeof(Meta::IndexBlock),
                                   reinterpret_cast<const char*>(&index)) < 0) {
                return false;
            }
        }
        return true;
    }

    bool IndexManagerImpl::appendIndexCatalog(const std::string& databaseName,
                                              const std::string& tableName,
                                              const Meta::IndexBlock& indexMeta) {
        const std::string tidFile = tableTidPath(databaseName, tableName);
        return storage_->appendRaw(tidFile, sizeof(Meta::IndexBlock),
                                   reinterpret_cast<const char*>(&indexMeta)) >= 0;
    }

    bool IndexManagerImpl::initializeIndexFile(const std::string& indexFile) {
        if (!storage_->createFile(indexFile)) return false;

        FileHeader header;
        std::memset(&header, 0, sizeof(header));
        copyCString(header.magic, sizeof(header.magic), kMagic);
        header.version = 1;
        header.rootPage = 0;
        header.nextPage = 1;
        header.nodeOrder = kNodeOrder;
        header.keyBytes = kMaxKeyBytes;

        TreeNode root;
        std::memset(&root, 0, sizeof(root));
        root.pageId = 0;
        root.isLeaf = true;
        root.keyCount = 0;
        root.parentPage = kInvalidPage;
        root.prevLeaf = kInvalidPage;
        root.nextLeaf = kInvalidPage;
        for (int& child : root.children) child = kInvalidPage;

        return saveHeader(indexFile, header) && writeNode(indexFile, root);
    }

    bool IndexManagerImpl::loadHeader(const std::string& indexFile, FileHeader& header) {
        if (storage_->getFileSize(indexFile) < static_cast<long>(sizeof(FileHeader))) return false;
        if (!storage_->readRaw(indexFile, 0, sizeof(FileHeader), reinterpret_cast<char*>(&header))) return false;
        return std::strncmp(header.magic, kMagic, sizeof(header.magic)) == 0 &&
               header.nodeOrder == kNodeOrder &&
               header.keyBytes == kMaxKeyBytes;
    }

    bool IndexManagerImpl::saveHeader(const std::string& indexFile, const FileHeader& header) {
        return storage_->writeRaw(indexFile, 0, sizeof(FileHeader), reinterpret_cast<const char*>(&header));
    }

    bool IndexManagerImpl::readNode(const std::string& indexFile, int pageId, TreeNode& node) {
        if (pageId < 0) return false;
        const long offset = sizeof(FileHeader) + static_cast<long>(pageId) * sizeof(TreeNode);
        return storage_->readRaw(indexFile, offset, sizeof(TreeNode), reinterpret_cast<char*>(&node));
    }

    bool IndexManagerImpl::writeNode(const std::string& indexFile, const TreeNode& node) {
        if (node.pageId < 0) return false;
        const long offset = sizeof(FileHeader) + static_cast<long>(node.pageId) * sizeof(TreeNode);
        return storage_->writeRaw(indexFile, offset, sizeof(TreeNode), reinterpret_cast<const char*>(&node));
    }

    bool IndexManagerImpl::allocateNode(const std::string& indexFile,
                                        FileHeader& header,
                                        bool leaf,
                                        TreeNode& node) {
        std::memset(&node, 0, sizeof(node));
        node.pageId = header.nextPage++;
        node.isLeaf = leaf;
        node.keyCount = 0;
        node.parentPage = kInvalidPage;
        node.prevLeaf = kInvalidPage;
        node.nextLeaf = kInvalidPage;
        for (int& child : node.children) child = kInvalidPage;
        return saveHeader(indexFile, header) && writeNode(indexFile, node);
    }

    IndexManagerImpl::KeyEntry IndexManagerImpl::makeEntry(const IndexKey& key, long recordOffset) const {
        KeyEntry entry;
        std::memset(&entry, 0, sizeof(entry));
        std::string packed;
        for (size_t i = 0; i < key.values.size(); ++i) {
            if (i > 0) packed.push_back('\x1F');
            packed += key.values[i];
        }
        const size_t copyLen = std::min(packed.size(), static_cast<size_t>(kMaxKeyBytes));
        if (copyLen > 0) {
            std::memcpy(entry.key, packed.data(), copyLen);
        }
        entry.recordOffset = recordOffset;
        return entry;
    }

    int IndexManagerImpl::compareKey(const KeyEntry& lhs, const KeyEntry& rhs) const {
        int keyCmp = std::memcmp(lhs.key, rhs.key, kMaxKeyBytes);
        if (keyCmp != 0) return keyCmp;
        if (lhs.recordOffset < rhs.recordOffset) return -1;
        if (lhs.recordOffset > rhs.recordOffset) return 1;
        return 0;
    }

    int IndexManagerImpl::compareSearchKey(const KeyEntry& lhs, const IndexKey& rhs) const {
        KeyEntry rhsEntry = makeEntry(rhs, lhs.recordOffset);
        return std::memcmp(lhs.key, rhsEntry.key, kMaxKeyBytes);
    }

    bool IndexManagerImpl::entryEquals(const KeyEntry& lhs,
                                       const IndexKey& key,
                                       long recordOffset) const {
        KeyEntry rhs = makeEntry(key, recordOffset);
        return compareKey(lhs, rhs) == 0;
    }

    bool IndexManagerImpl::keyExists(const std::string& indexFile, const IndexKey& key) {
        FileHeader header;
        if (!loadHeader(indexFile, header) || header.rootPage == kInvalidPage) return false;

        int leafPage = findLeftmostLeafPage(indexFile, header, key);
        while (leafPage != kInvalidPage) {
            TreeNode leaf;
            if (!readNode(indexFile, leafPage, leaf)) return false;

            for (int i = 0; i < leaf.keyCount; ++i) {
                int cmp = compareSearchKey(leaf.entries[i], key);
                if (cmp == 0) return true;
                if (cmp > 0) return false;
            }
            leafPage = leaf.nextLeaf;
        }
        return false;
    }

    bool IndexManagerImpl::insertEntryInternal(const std::string& indexFile,
                                               const IndexKey& key,
                                               const KeyEntry& entry,
                                               bool unique) {
        FileHeader header;
        if (!loadHeader(indexFile, header)) return false;

        if (unique && keyExists(indexFile, key)) return false;

        int leafPage = findLeafPage(indexFile, header, key);
        if (leafPage == kInvalidPage) return false;

        TreeNode leaf;
        if (!readNode(indexFile, leafPage, leaf)) return false;

        KeyEntry promotedKey;
        int promotedPage = kInvalidPage;
        bool ok = insertIntoLeaf(indexFile, header, leaf, entry, unique, promotedKey, promotedPage);
        if (!ok) return false;

        if (promotedPage != kInvalidPage) {
            TreeNode right;
            if (!readNode(indexFile, promotedPage, right)) return false;
            return insertIntoParent(indexFile, header, leaf, promotedKey, right);
        }
        return true;
    }

    bool IndexManagerImpl::insertIntoLeaf(const std::string& indexFile,
                                          FileHeader& header,
                                          TreeNode& leaf,
                                          const KeyEntry& entry,
                                          bool unique,
                                          KeyEntry& promotedKey,
                                          int& promotedPage) {
        promotedPage = kInvalidPage;
        int pos = 0;
        while (pos < leaf.keyCount && compareKey(leaf.entries[pos], entry) < 0) ++pos;

        for (int i = leaf.keyCount; i > pos; --i) {
            leaf.entries[i] = leaf.entries[i - 1];
        }
        leaf.entries[pos] = entry;
        ++leaf.keyCount;

        if (leaf.keyCount <= kNodeOrder) {
            return writeNode(indexFile, leaf);
        }

        TreeNode right;
        if (!allocateNode(indexFile, header, true, right)) return false;
        right.parentPage = leaf.parentPage;

        const int split = leaf.keyCount / 2;
        const int rightCount = leaf.keyCount - split;
        for (int i = 0; i < rightCount; ++i) {
            right.entries[i] = leaf.entries[split + i];
        }
        right.keyCount = rightCount;
        leaf.keyCount = split;
        right.prevLeaf = leaf.pageId;
        right.nextLeaf = leaf.nextLeaf;
        if (right.nextLeaf != kInvalidPage) {
            TreeNode next;
            if (!readNode(indexFile, right.nextLeaf, next)) return false;
            next.prevLeaf = right.pageId;
            if (!writeNode(indexFile, next)) return false;
        }
        leaf.nextLeaf = right.pageId;

        promotedKey = right.entries[0];
        promotedPage = right.pageId;

        return writeNode(indexFile, leaf) && writeNode(indexFile, right);
    }

    bool IndexManagerImpl::insertIntoParent(const std::string& indexFile,
                                            FileHeader& header,
                                            TreeNode& left,
                                            const KeyEntry& key,
                                            TreeNode& right) {
        if (left.parentPage == kInvalidPage) {
            TreeNode root;
            if (!allocateNode(indexFile, header, false, root)) return false;

            root.keyCount = 1;
            root.entries[0] = key;
            root.children[0] = left.pageId;
            root.children[1] = right.pageId;
            root.parentPage = kInvalidPage;
            header.rootPage = root.pageId;
            left.parentPage = root.pageId;
            right.parentPage = root.pageId;

            return saveHeader(indexFile, header) &&
                   writeNode(indexFile, root) &&
                   writeNode(indexFile, left) &&
                   writeNode(indexFile, right);
        }

        TreeNode parent;
        if (!readNode(indexFile, left.parentPage, parent)) return false;

        int childPos = 0;
        while (childPos <= parent.keyCount && parent.children[childPos] != left.pageId) ++childPos;
        if (childPos > parent.keyCount) return false;

        for (int i = parent.keyCount; i > childPos; --i) {
            parent.entries[i] = parent.entries[i - 1];
        }
        for (int i = parent.keyCount + 1; i > childPos + 1; --i) {
            parent.children[i] = parent.children[i - 1];
        }

        parent.entries[childPos] = key;
        parent.children[childPos + 1] = right.pageId;
        ++parent.keyCount;
        right.parentPage = parent.pageId;

        if (parent.keyCount <= kNodeOrder) {
            return writeNode(indexFile, parent) && writeNode(indexFile, right);
        }

        KeyEntry promotedKey;
        int promotedPage = kInvalidPage;
        if (!splitInternalNode(indexFile, header, parent, promotedKey, promotedPage)) return false;

        TreeNode promotedRight;
        if (!readNode(indexFile, promotedPage, promotedRight)) return false;
        return insertIntoParent(indexFile, header, parent, promotedKey, promotedRight);
    }

    bool IndexManagerImpl::splitInternalNode(const std::string& indexFile,
                                             FileHeader& header,
                                             TreeNode& node,
                                             KeyEntry& promotedKey,
                                             int& promotedPage) {
        TreeNode right;
        if (!allocateNode(indexFile, header, false, right)) return false;

        const int mid = node.keyCount / 2;
        promotedKey = node.entries[mid];
        right.parentPage = node.parentPage;

        int rightKeyCount = node.keyCount - mid - 1;
        for (int i = 0; i < rightKeyCount; ++i) {
            right.entries[i] = node.entries[mid + 1 + i];
        }
        for (int i = 0; i <= rightKeyCount; ++i) {
            right.children[i] = node.children[mid + 1 + i];
            if (right.children[i] != kInvalidPage) {
                TreeNode child;
                if (!readNode(indexFile, right.children[i], child)) return false;
                child.parentPage = right.pageId;
                if (!writeNode(indexFile, child)) return false;
            }
        }

        node.keyCount = mid;
        right.keyCount = rightKeyCount;
        promotedPage = right.pageId;

        return writeNode(indexFile, node) && writeNode(indexFile, right);
    }

    int IndexManagerImpl::findLeafPage(const std::string& indexFile,
                                       const FileHeader& header,
                                       const IndexKey& key) {
        int pageId = header.rootPage;
        while (pageId != kInvalidPage) {
            TreeNode node;
            if (!readNode(indexFile, pageId, node)) return kInvalidPage;
            if (node.isLeaf) return pageId;

            int pos = 0;
            while (pos < node.keyCount && compareSearchKey(node.entries[pos], key) <= 0) ++pos;
            pageId = node.children[pos];
        }
        return kInvalidPage;
    }

    int IndexManagerImpl::findLeftmostLeafPage(const std::string& indexFile,
                                               const FileHeader& header,
                                               const IndexKey& key) {
        int leafPage = findLeafPage(indexFile, header, key);
        if (leafPage == kInvalidPage) return kInvalidPage;

        while (true) {
            TreeNode leaf;
            if (!readNode(indexFile, leafPage, leaf)) return kInvalidPage;
            if (leaf.prevLeaf == kInvalidPage) return leafPage;

            TreeNode prev;
            if (!readNode(indexFile, leaf.prevLeaf, prev)) return kInvalidPage;
            if (prev.keyCount == 0) {
                leafPage = leaf.prevLeaf;
                continue;
            }
            if (compareSearchKey(prev.entries[prev.keyCount - 1], key) < 0) {
                return leafPage;
            }
            leafPage = leaf.prevLeaf;
        }
    }

}
