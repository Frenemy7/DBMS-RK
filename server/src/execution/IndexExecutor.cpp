#include "IndexExecutor.h"
#include "IndexMaintenance.h"
#include "../index/IndexManagerImpl.h"

#include <algorithm>
#include <iostream>
#include <set>

namespace Execution {

namespace {

bool tableHasField(const std::vector<Meta::FieldBlock>& fields, const std::string& name) {
    return std::any_of(fields.begin(), fields.end(), [&](const Meta::FieldBlock& field) {
        return std::string(field.name) == name;
    });
}

} // namespace

CreateIndexExecutor::CreateIndexExecutor(std::unique_ptr<Parser::CreateIndexASTNode> ast,
                                         Catalog::ICatalogManager* catalog,
                                         Storage::IStorageEngine* storage,
                                         Index::IIndexManager* indexManager)
    : astNode_(std::move(ast)),
      catalog_(catalog),
      storage_(storage),
      indexManager_(indexManager) {}

Index::IIndexManager* CreateIndexExecutor::indexManager() {
    if (indexManager_) return indexManager_;
    if (!ownedIndexManager_) {
        ownedIndexManager_ = std::make_unique<Index::IndexManagerImpl>(storage_);
    }
    return ownedIndexManager_.get();
}

bool CreateIndexExecutor::execute() {
    if (!astNode_ || !catalog_ || !storage_) return false;

    const std::string databaseName = catalog_->getCurrentDatabase();
    if (databaseName.empty()) {
        std::cerr << "Error: no database selected." << std::endl;
        return false;
    }
    if (!catalog_->hasTable(astNode_->tableName)) {
        std::cerr << "Error: table '" << astNode_->tableName << "' does not exist." << std::endl;
        return false;
    }
    if (astNode_->fields.empty() || astNode_->fields.size() > 2) {
        std::cerr << "Error: index supports 1 or 2 fields." << std::endl;
        return false;
    }

    auto fields = catalog_->getFields(astNode_->tableName);
    std::set<std::string> seenFields;
    std::vector<std::string> indexFields;
    for (const auto& field : astNode_->fields) {
        if (!tableHasField(fields, field.fieldName)) {
            std::cerr << "Error: field '" << field.fieldName << "' does not exist." << std::endl;
            return false;
        }
        if (!seenFields.insert(field.fieldName).second) {
            std::cerr << "Error: duplicate index field '" << field.fieldName << "'." << std::endl;
            return false;
        }
        indexFields.push_back(field.fieldName);
    }

    Meta::TableBlock tableMeta = catalog_->getTableMeta(astNode_->tableName);
    Index::IndexCreateOptions options;
    options.databaseName = databaseName;
    options.tableName = astNode_->tableName;
    options.indexName = astNode_->indexName;
    options.fields = indexFields;
    options.unique = astNode_->unique;
    options.asc = astNode_->fields.empty() ? true : astNode_->fields.front().asc;
    options.recordFile = "data/" + databaseName + "/" + std::string(tableMeta.trd);

    Index::IIndexManager* manager = indexManager();
    if (!manager || !manager->createIndex(options)) {
        std::cerr << "Error: create index failed." << std::endl;
        return false;
    }

    auto indices = manager->getIndices(databaseName, astNode_->tableName);
    auto it = std::find_if(indices.begin(), indices.end(), [&](const Meta::IndexBlock& indexMeta) {
        return std::string(indexMeta.name) == astNode_->indexName;
    });
    if (it == indices.end()) {
        std::cerr << "Error: index metadata missing after create." << std::endl;
        return false;
    }

    if (!IndexMaintenance::rebuildTableIndex(astNode_->tableName, *it, catalog_, storage_, manager)) {
        manager->dropIndex(databaseName, astNode_->tableName, astNode_->indexName);
        std::cerr << "Error: build index entries failed." << std::endl;
        return false;
    }

    std::cout << "Query OK. Index created." << std::endl;
    return true;
}

DropIndexExecutor::DropIndexExecutor(std::unique_ptr<Parser::DropIndexASTNode> ast,
                                     Catalog::ICatalogManager* catalog,
                                     Storage::IStorageEngine* storage,
                                     Index::IIndexManager* indexManager)
    : astNode_(std::move(ast)),
      catalog_(catalog),
      storage_(storage),
      indexManager_(indexManager) {}

Index::IIndexManager* DropIndexExecutor::indexManager() {
    if (indexManager_) return indexManager_;
    if (!ownedIndexManager_) {
        ownedIndexManager_ = std::make_unique<Index::IndexManagerImpl>(storage_);
    }
    return ownedIndexManager_.get();
}

bool DropIndexExecutor::execute() {
    if (!astNode_ || !catalog_ || !storage_) return false;

    const std::string databaseName = catalog_->getCurrentDatabase();
    if (databaseName.empty()) {
        std::cerr << "Error: no database selected." << std::endl;
        return false;
    }
    if (!catalog_->hasTable(astNode_->tableName)) {
        std::cerr << "Error: table '" << astNode_->tableName << "' does not exist." << std::endl;
        return false;
    }

    Index::IIndexManager* manager = indexManager();
    if (!manager || !manager->dropIndex(databaseName, astNode_->tableName, astNode_->indexName)) {
        std::cerr << "Error: drop index failed." << std::endl;
        return false;
    }

    std::cout << "Query OK. Index dropped." << std::endl;
    return true;
}

} // namespace Execution
