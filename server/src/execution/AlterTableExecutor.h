#ifndef ALTER_TABLE_EXECUTOR_H
#define ALTER_TABLE_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/parser/ASTNode.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/meta/TableMeta.h"
#include <string>
#include <vector>

namespace Execution {

    class AlterTableExecutor : public IExecutor {
    private:
        std::unique_ptr<Parser::AlterTableASTNode> astNode_;
        Catalog::ICatalogManager* catalogManager_;

        bool executeAddColumn();
        bool executeDropColumn();
        bool executeModifyColumn();

        // 记录重写引擎：需求 3.4.2/3.4.3 要求字段变更后更新全部记录
        bool hasRecords(const std::string& tableName);
        int  calcFieldSize(const Meta::FieldBlock& f);
        int  calcRecordSize(const std::vector<Meta::FieldBlock>& fields,
                            std::vector<int>& offsets);
        bool rewriteRecords(const std::string& tableName,
                            const std::vector<Meta::FieldBlock>& oldFields,
                            const std::vector<Meta::FieldBlock>& newFields,
                            int alterFieldIndex = -1, const std::string& alterOp = "");
        bool readTrd(const std::string& path, std::vector<char>& data);
        bool writeTrd(const std::string& path, const char* data, int size);

    public:
        AlterTableExecutor(std::unique_ptr<Parser::AlterTableASTNode> ast,
                          Catalog::ICatalogManager* catalog);
        ~AlterTableExecutor() override = default;
        bool execute() override;
    };

}
#endif
