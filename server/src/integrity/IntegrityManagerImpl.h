#ifndef INTEGRITY_MANAGER_IMPL_H
#define INTEGRITY_MANAGER_IMPL_H

#include "../../include/integrity/IIntegrityManager.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/meta/TableMeta.h"
#include "../../include/storage/IStorageEngine.h"

#include <map>
#include <string>
#include <vector>

namespace Integrity {

    class IntegrityManagerImpl : public IIntegrityManager {
    private:
        Catalog::ICatalogManager* catalog;
        Storage::IStorageEngine* storage;

        bool checkNotNull(const std::string& value) const;
        bool checkPrimaryKey(const std::string& tableName,
                             const std::string& fieldName,
                             const std::string& value);
        bool checkUnique(const std::string& tableName,
                         const std::string& fieldName,
                         const std::string& value);
        bool checkForeignKey(const std::string& fieldValue,
                             const std::string& referencedTable,
                             const std::string& referencedField);
        // CHECK 约束求值
        bool checkConstraint(const std::string& condition,
                             const std::map<std::string, std::string>& valueMap,
                             const std::vector<Meta::FieldBlock>& fields);

        std::map<std::string, std::string> buildValueMap(
            const std::string& tableName,
            const std::vector<std::string>& columns,
            const std::vector<std::string>& values);

        bool fieldExists(const std::string& tableName, const std::string& fieldName);
        std::vector<std::vector<std::string>> readRows(const std::string& tableName);
        std::string getFieldValue(const std::vector<std::string>& row,
                                  const std::vector<Meta::FieldBlock>& fields,
                                  const std::string& fieldName) const;
        std::vector<std::string> listCurrentDatabaseTables();
    public:
        IntegrityManagerImpl(Catalog::ICatalogManager* catalogManager,
                             Storage::IStorageEngine* storageEngine);

        bool checkInsert(const std::string& tableName,
                         const std::vector<std::string>& columns,
                         const std::vector<std::string>& values) override;

        bool checkUpdate(const std::string& tableName,
                         const std::string& column,
                         const std::string& value) override;

        bool checkDelete(const std::string& tableName,
                         const std::string& pkValue) override;
    };

}

#endif // INTEGRITY_MANAGER_IMPL_H
