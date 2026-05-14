#ifndef INSERT_EXECUTOR_H
#define INSERT_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
#include "../../include/integrity/IIntegrityManager.h"
#include "../../include/parser/InsertASTNode.h" 
#include <memory>
#include <vector>
#include <string>

namespace Execution {

    class InsertExecutor : public IExecutor {
    private:
        std::unique_ptr<Parser::InsertASTNode> astNode;
        Catalog::ICatalogManager* catalogManager;
        Storage::IStorageEngine* storageEngine;
        Integrity::IIntegrityManager* integrityManager;

        // 单行插入（INSERT VALUES 和 INSERT SELECT 共用）
        bool insertSingleRow(const std::string& tableName,
                             const std::vector<std::string>& insertValues,
                             const std::vector<std::string>& insertCols);

    public:
        InsertExecutor(std::unique_ptr<Parser::InsertASTNode> ast,
                       Catalog::ICatalogManager* catalog,
                       Storage::IStorageEngine* storage,
                       Integrity::IIntegrityManager* integrity);

        ~InsertExecutor() override = default;

        bool execute() override;
    };

}

#endif // INSERT_EXECUTOR_H