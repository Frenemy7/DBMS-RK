#include "CatalogManagerImpl.h"
#include <iostream>
#include <cstring>
#include <vector>
#include <string>

namespace Catalog {

    CatalogManagerImpl::CatalogManagerImpl(Storage::IStorageEngine* storage)
        : storageEngine(storage), currentDB("") {}

    CatalogManagerImpl::~CatalogManagerImpl() {}

    bool CatalogManagerImpl::initSystem() {
        if (storageEngine == nullptr) return false;

        // data 目录存在也算成功
        if (!storageEngine->createDirectory("data")) {
            return false;
        }

        // ruanko.db 不存在才创建
        if (storageEngine->getFileSize("data/ruanko.db") < 0) {
            if (!storageEngine->createFile("data/ruanko.db")) {
                return false;
            }
        }

        return true;
    }

    bool CatalogManagerImpl::useDatabase(const std::string& dbName) {
        if (!hasDatabase(dbName)) {
            return false;
        }
        currentDB = dbName;
        return true;
    }

    std::string CatalogManagerImpl::getCurrentDatabase() {
        return currentDB;
    }

    bool CatalogManagerImpl::hasDatabase(const std::string& dbName) {
        auto dbs = getAllDatabases();
        for (const auto& db : dbs) {
            if (std::string(db.name) == dbName) {
                return true;
            }
        }
        return false;
    }

    Meta::DatabaseBlock CatalogManagerImpl::getDatabaseMeta(const std::string& dbName) {
        auto dbs = getAllDatabases();
        for (const auto& db : dbs) {
            if (std::string(db.name) == dbName) {
                return db;
            }
        }

        Meta::DatabaseBlock empty;
        std::memset(&empty, 0, sizeof(empty));
        return empty;
    }

    std::vector<Meta::DatabaseBlock> CatalogManagerImpl::getAllDatabases() {
        std::vector<Meta::DatabaseBlock> dbs;

        long fileSize = storageEngine->getFileSize("data/ruanko.db");
        if (fileSize <= 0) return dbs;

        int blockSize = sizeof(Meta::DatabaseBlock);
        if (fileSize % blockSize != 0) return dbs;

        std::vector<char> buffer(fileSize);
        if (!storageEngine->readRaw("data/ruanko.db", 0, static_cast<int>(fileSize), buffer.data())) {
            return dbs;
        }

        int numBlocks = static_cast<int>(fileSize / blockSize);
        for (int i = 0; i < numBlocks; ++i) {
            Meta::DatabaseBlock db;
            std::memcpy(&db, buffer.data() + i * blockSize, blockSize);
            dbs.push_back(db);
        }

        return dbs;
    }

    bool CatalogManagerImpl::createDatabase(const Meta::DatabaseBlock& dbMeta) {
        if (storageEngine == nullptr) return false;
        if (hasDatabase(dbMeta.name)) return false;

        std::string dbDir = "data/" + std::string(dbMeta.name);

        // 先创建目录
        if (!storageEngine->createDirectory(dbDir)) {
            return false;
        }

        // 再登记到 ruanko.db
        long offset = storageEngine->appendRaw(
            "data/ruanko.db",
            sizeof(Meta::DatabaseBlock),
            reinterpret_cast<const char*>(&dbMeta)
        );

        if (offset < 0) {
            // 回滚
            storageEngine->deleteDirectory(dbDir);
            return false;
        }

        return true;
    }

    bool CatalogManagerImpl::dropDatabase(const std::string& dbName) {
        if (storageEngine == nullptr) return false;

        auto dbs = getAllDatabases();
        std::vector<Meta::DatabaseBlock> newDbs;
        bool found = false;

        for (const auto& db : dbs) {
            if (std::string(db.name) != dbName) {
                newDbs.push_back(db);
            } else {
                found = true;
            }
        }

        if (!found) return false;

        // 如果当前正在使用这个数据库，先清掉上下文
        if (currentDB == dbName) {
            currentDB.clear();
        }
    

        // 先删目录
        std::string dbDir = "data/" + dbName;
        if (!storageEngine->deleteDirectory(dbDir)) {
            return false;
        }

        // 再重写 ruanko.db
        if (!storageEngine->clearFile("data/ruanko.db")) {
            return false;
        }

        for (const auto& db : newDbs) {
            if (storageEngine->appendRaw(
                    "data/ruanko.db",
                    sizeof(Meta::DatabaseBlock),
                    reinterpret_cast<const char*>(&db)) < 0) {
                return false;
            }
        }

        return true;
    }

    bool CatalogManagerImpl::hasTable(const std::string& tableName) {
        if (currentDB.empty()) return false;

        std::string tbFile = "data/" + currentDB + "/" + tableName + ".tb";
        long size = storageEngine->getFileSize(tbFile);
        return size == static_cast<long>(sizeof(Meta::TableBlock));
    }

    Meta::TableBlock CatalogManagerImpl::getTableMeta(const std::string& tableName) {
        Meta::TableBlock tb;
        std::memset(&tb, 0, sizeof(tb));

        if (currentDB.empty()) return tb;

        std::string tbFile = "data/" + currentDB + "/" + tableName + ".tb";
        long size = storageEngine->getFileSize(tbFile);
        if (size != static_cast<long>(sizeof(Meta::TableBlock))) {
            return tb;
        }

        if (storageEngine->readRaw(
                tbFile,
                0,
                sizeof(Meta::TableBlock),
                reinterpret_cast<char*>(&tb))) {
            return tb;
        }

        std::memset(&tb, 0, sizeof(tb));
        return tb;
    }

    bool CatalogManagerImpl::createTable(const Meta::TableBlock& tableMeta,
                                         const std::vector<Meta::FieldBlock>& fields) {
        if (storageEngine == nullptr) return false;
        if (currentDB.empty()) return false;
        if (hasTable(tableMeta.name)) return false;

        std::string dbDir   = "data/" + currentDB;
        std::string tbFile  = dbDir + "/" + std::string(tableMeta.name) + ".tb";
        std::string tdfFile = dbDir + "/" + std::string(tableMeta.tdf);
        std::string trdFile = dbDir + "/" + std::string(tableMeta.trd);
        std::string ticFile = dbDir + "/" + std::string(tableMeta.tic);
        std::string tidFile = dbDir + "/" + std::string(tableMeta.tid);

        if (!storageEngine->createDirectory(dbDir)) return false;

        if (!storageEngine->createFile(tbFile)) return false;
        if (!storageEngine->writeRaw(
                tbFile,
                0,
                sizeof(Meta::TableBlock),
                reinterpret_cast<const char*>(&tableMeta))) {
            return false;
        }

        if (!storageEngine->createFile(tdfFile)) return false;
        for (const auto& field : fields) {
            if (storageEngine->appendRaw(
                    tdfFile,
                    sizeof(Meta::FieldBlock),
                    reinterpret_cast<const char*>(&field)) < 0) {
                return false;
            }
        }

        if (!storageEngine->createFile(trdFile)) return false;
        if (!storageEngine->createFile(ticFile)) return false;
        if (!storageEngine->createFile(tidFile)) return false;

        return true;
    }

    bool CatalogManagerImpl::dropTable(const std::string& tableName) {
        if (currentDB.empty() || !hasTable(tableName)) return false;

        Meta::TableBlock tb = getTableMeta(tableName);
        std::string dbDir = "data/" + currentDB;

        bool ok = true;
        ok = storageEngine->deleteFile(dbDir + "/" + tableName + ".tb") && ok;
        ok = storageEngine->deleteFile(dbDir + "/" + std::string(tb.tdf)) && ok;
        ok = storageEngine->deleteFile(dbDir + "/" + std::string(tb.trd)) && ok;
        ok = storageEngine->deleteFile(dbDir + "/" + std::string(tb.tic)) && ok;
        ok = storageEngine->deleteFile(dbDir + "/" + std::string(tb.tid)) && ok;

        return ok;
    }

    bool CatalogManagerImpl::updateTableMeta(const std::string& tableName,
                                             const Meta::TableBlock& newTableMeta) {
        if (currentDB.empty() || !hasTable(tableName)) return false;

        std::string tbFile = "data/" + currentDB + "/" + tableName + ".tb";
        return storageEngine->writeRaw(
            tbFile,
            0,
            sizeof(Meta::TableBlock),
            reinterpret_cast<const char*>(&newTableMeta)
        );
    }

    std::vector<Meta::FieldBlock> CatalogManagerImpl::getFields(const std::string& tableName) {
        std::vector<Meta::FieldBlock> fields;
        if (currentDB.empty() || !hasTable(tableName)) return fields;

        Meta::TableBlock tb = getTableMeta(tableName);
        std::string tdfFile = "data/" + currentDB + "/" + std::string(tb.tdf);

        long fileSize = storageEngine->getFileSize(tdfFile);
        if (fileSize <= 0) return fields;

        int blockSize = sizeof(Meta::FieldBlock);
        if (fileSize % blockSize != 0) return fields;

        std::vector<char> buffer(fileSize);
        if (!storageEngine->readRaw(tdfFile, 0, static_cast<int>(fileSize), buffer.data())) {
            return fields;
        }

        int numBlocks = static_cast<int>(fileSize / blockSize);
        for (int i = 0; i < numBlocks; ++i) {
            Meta::FieldBlock field;
            std::memcpy(&field, buffer.data() + i * blockSize, blockSize);
            fields.push_back(field);
        }

        return fields;
    }

    bool CatalogManagerImpl::updateField(const std::string& tableName, const Meta::FieldBlock& field) {
        if (currentDB.empty() || !hasTable(tableName)) return false;

        auto fields = getFields(tableName);
        Meta::TableBlock tb = getTableMeta(tableName);
        std::string tdfFile = "data/" + currentDB + "/" + std::string(tb.tdf);

        for (size_t i = 0; i < fields.size(); ++i) {
            if (std::string(fields[i].name) == std::string(field.name)) {
                return storageEngine->writeRaw(
                    tdfFile,
                    static_cast<long>(i * sizeof(Meta::FieldBlock)),
                    sizeof(Meta::FieldBlock),
                    reinterpret_cast<const char*>(&field)
                );
            }
        }

        return false;
    }

    bool CatalogManagerImpl::addField(const std::string& tableName, const Meta::FieldBlock& field) {
        if (currentDB.empty() || !hasTable(tableName)) return false;

        Meta::TableBlock tb = getTableMeta(tableName);
        std::string tdfFile = "data/" + currentDB + "/" + std::string(tb.tdf);

        long offset = storageEngine->appendRaw(
            tdfFile,
            sizeof(Meta::FieldBlock),
            reinterpret_cast<const char*>(&field)
        );
        if (offset < 0) return false;

        tb.field_num++;
        return updateTableMeta(tableName, tb);
    }

    bool CatalogManagerImpl::dropField(const std::string& tableName, const std::string& fieldName) {
        if (currentDB.empty() || !hasTable(tableName)) return false;

        auto fields = getFields(tableName);
        std::vector<Meta::FieldBlock> newFields;
        bool found = false;

        for (const auto& f : fields) {
            if (std::string(f.name) != fieldName) {
                newFields.push_back(f);
            } else {
                found = true;
            }
        }

        if (!found) return false;

        Meta::TableBlock tb = getTableMeta(tableName);
        std::string tdfFile = "data/" + currentDB + "/" + std::string(tb.tdf);

        if (!storageEngine->clearFile(tdfFile)) return false;

        for (const auto& f : newFields) {
            if (storageEngine->appendRaw(
                    tdfFile,
                    sizeof(Meta::FieldBlock),
                    reinterpret_cast<const char*>(&f)) < 0) {
                return false;
            }
        }

        tb.field_num--;
        return updateTableMeta(tableName, tb);
    }

    std::vector<Meta::IntegrityBlock> CatalogManagerImpl::getIntegrities(const std::string& tableName) {
        std::vector<Meta::IntegrityBlock> integrities;
        if (currentDB.empty() || !hasTable(tableName)) return integrities;

        Meta::TableBlock tb = getTableMeta(tableName);
        std::string ticFile = "data/" + currentDB + "/" + std::string(tb.tic);

        long fileSize = storageEngine->getFileSize(ticFile);
        if (fileSize <= 0) return integrities;

        int blockSize = sizeof(Meta::IntegrityBlock);
        if (fileSize % blockSize != 0) return integrities;

        std::vector<char> buffer(fileSize);
        if (!storageEngine->readRaw(ticFile, 0, static_cast<int>(fileSize), buffer.data())) {
            return integrities;
        }

        int numBlocks = static_cast<int>(fileSize / blockSize);
        for (int i = 0; i < numBlocks; ++i) {
            Meta::IntegrityBlock integrity;
            std::memcpy(&integrity, buffer.data() + i * blockSize, blockSize);
            integrities.push_back(integrity);
        }

        return integrities;
    }

    std::vector<Meta::IndexBlock> CatalogManagerImpl::getIndices(const std::string& tableName) {
        std::vector<Meta::IndexBlock> indices;
        if (currentDB.empty() || !hasTable(tableName)) return indices;

        Meta::TableBlock tb = getTableMeta(tableName);
        std::string tidFile = "data/" + currentDB + "/" + std::string(tb.tid);

        long fileSize = storageEngine->getFileSize(tidFile);
        if (fileSize <= 0) return indices;

        int blockSize = sizeof(Meta::IndexBlock);
        if (fileSize % blockSize != 0) return indices;

        std::vector<char> buffer(fileSize);
        if (!storageEngine->readRaw(tidFile, 0, static_cast<int>(fileSize), buffer.data())) {
            return indices;
        }

        int numBlocks = static_cast<int>(fileSize / blockSize);
        for (int i = 0; i < numBlocks; ++i) {
            Meta::IndexBlock index;
            std::memcpy(&index, buffer.data() + i * blockSize, blockSize);
            indices.push_back(index);
        }

        return indices;
    }

    bool CatalogManagerImpl::addIndex(const std::string& tableName, const Meta::IndexBlock& indexMeta) {
        if (currentDB.empty() || !hasTable(tableName)) return false;

        Meta::TableBlock tb = getTableMeta(tableName);
        std::string tidFile = "data/" + currentDB + "/" + std::string(tb.tid);

        return storageEngine->appendRaw(
            tidFile,
            sizeof(Meta::IndexBlock),
            reinterpret_cast<const char*>(&indexMeta)
        ) >= 0;
    }

    bool CatalogManagerImpl::dropIndex(const std::string& tableName, const std::string& indexName) {
        if (currentDB.empty() || !hasTable(tableName)) return false;

        auto indices = getIndices(tableName);
        std::vector<Meta::IndexBlock> newIndices;
        bool found = false;

        for (const auto& idx : indices) {
            if (std::string(idx.name) != indexName) {
                newIndices.push_back(idx);
            } else {
                found = true;
            }
        }

        if (!found) return false;

        Meta::TableBlock tb = getTableMeta(tableName);
        std::string tidFile = "data/" + currentDB + "/" + std::string(tb.tid);

        if (!storageEngine->clearFile(tidFile)) return false;

        for (const auto& idx : newIndices) {
            if (storageEngine->appendRaw(
                    tidFile,
                    sizeof(Meta::IndexBlock),
                    reinterpret_cast<const char*>(&idx)) < 0) {
                return false;
            }
        }

        return true;
    }

    bool CatalogManagerImpl::addIntegrity(const std::string& tableName,
                                          const Meta::IntegrityBlock& integrityMeta) {
        if (currentDB.empty() || !hasTable(tableName)) return false;

        Meta::TableBlock tb = getTableMeta(tableName);
        std::string ticFile = "data/" + currentDB + "/" + std::string(tb.tic);

        return storageEngine->appendRaw(
            ticFile,
            sizeof(Meta::IntegrityBlock),
            reinterpret_cast<const char*>(&integrityMeta)
        ) >= 0;
    }

    bool CatalogManagerImpl::dropIntegrity(const std::string& tableName,
                                           const std::string& integrityName) {
        if (currentDB.empty() || !hasTable(tableName)) return false;

        auto integrities = getIntegrities(tableName);
        std::vector<Meta::IntegrityBlock> newIntegrities;
        bool found = false;

        for (const auto& intg : integrities) {
            if (std::string(intg.name) != integrityName) {
                newIntegrities.push_back(intg);
            } else {
                found = true;
            }
        }

        if (!found) return false;

        Meta::TableBlock tb = getTableMeta(tableName);
        std::string ticFile = "data/" + currentDB + "/" + std::string(tb.tic);

        if (!storageEngine->clearFile(ticFile)) return false;

        for (const auto& intg : newIntegrities) {
            if (storageEngine->appendRaw(
                    ticFile,
                    sizeof(Meta::IntegrityBlock),
                    reinterpret_cast<const char*>(&intg)) < 0) {
                return false;
            }
        }

        return true;
    }

} // namespace Catalog
