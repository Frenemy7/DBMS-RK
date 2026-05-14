#ifndef UPDATE_EXECUTOR_H
#define UPDATE_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
#include "../../include/integrity/IIntegrityManager.h"
#include "../../include/index/IIndexManager.h"
#include "../../include/parser/UpdateASTNode.h"
#include "../../include/meta/TableMeta.h"
#include <memory>
#include <vector>
#include <string>

namespace Execution {

    class UpdateExecutor : public IExecutor {
    private:
        std::unique_ptr<Parser::UpdateASTNode> astNode;
        Catalog::ICatalogManager* catalog;
        Storage::IStorageEngine* storage;
        Integrity::IIntegrityManager* integrity;
        Index::IIndexManager* indexManager_;
        std::unique_ptr<Index::IIndexManager> ownedIndexManager_;

        Index::IIndexManager* indexManager();

        // 表达式求值（支持列引用、字面量、算术运算）
        std::string evalExpr(Parser::ASTNode* node, const std::vector<std::string>& row,
                             const std::vector<Meta::FieldBlock>& schema);
        int findCol(const std::string& name, const std::vector<Meta::FieldBlock>& schema);

    public:
        UpdateExecutor(std::unique_ptr<Parser::UpdateASTNode> ast,
                       Catalog::ICatalogManager* cat,
                       Storage::IStorageEngine* stor,
                       Integrity::IIntegrityManager* integ,
                       Index::IIndexManager* index = nullptr);
        ~UpdateExecutor() override;
        bool execute() override;
    };

}
#endif
