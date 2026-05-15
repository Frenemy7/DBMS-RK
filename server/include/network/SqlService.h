#ifndef SQL_SERVICE_H
#define SQL_SERVICE_H

#include "../catalog/ICatalogManager.h"
#include "../storage/IStorageEngine.h"
#include "../integrity/IIntegrityManager.h"
#include "../index/IIndexManager.h"
#include "../maintenance/IDatabaseMaintenance.h"
#include "../security/ISecurityManager.h"
#include "../meta/TableMeta.h"
#include "../parser/ASTNode.h"
#include "../parser/SelectASTNode.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Network {

    struct QueryTable {
        std::vector<std::string> headers;
        std::vector<std::vector<std::string>> rows;
    };

    struct SqlResult {
        bool success = false;
        std::string text;
        std::string currentDatabase;
        QueryTable table;
    };

    class SqlService {
    public:
        SqlService(Catalog::ICatalogManager* catalog,
                   Storage::IStorageEngine* storage,
                   Integrity::IIntegrityManager* integrity,
                   Maintenance::IDatabaseMaintenance* maintenance,
                   Security::ISecurityManager* security,
                   Index::IIndexManager* index);

        SqlResult execute(const std::string& sql, const std::string& username);
        bool login(const std::string& username, const std::string& password);

    private:
        Catalog::ICatalogManager* catalog_;
        Storage::IStorageEngine* storage_;
        Integrity::IIntegrityManager* integrity_;
        Maintenance::IDatabaseMaintenance* maintenance_;
        Security::ISecurityManager* security_;
        Index::IIndexManager* index_;
        std::mutex executionMutex_;

        bool isAdminOnly(Parser::SQLType type) const;
        QueryTable buildSelectTable(Parser::ASTNode* ast);
        QueryTable runSelect(Parser::SelectASTNode* selectAst);
        std::string encodeResponse(const SqlResult& result) const;
    };

}

#endif // SQL_SERVICE_H
