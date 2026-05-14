#include "ExecutorFactory.h"
#include "AlterTableExecutor.h"
#include "BackupRestoreExecutor.h"
#include "UserExecutor.h"
#include "CreateExecutor.h"
#include "InsertExecutor.h"
#include "SelectExecutor.h"
#include "DeleteExecutor.h"
#include "DropTableExecutor.h"
#include "UpdateExecutor.h"
#include "CreateDatabaseExecutor.h"
#include "DropDatabaseExecutor.h"
#include "UseDatabaseExecutor.h"
#include "IndexExecutor.h"
#include "../../include/parser/CreateTableASTNode.h"
#include "../../include/parser/InsertASTNode.h"
#include "../../include/parser/SelectASTNode.h"
#include "../../include/parser/DeleteASTNode.h"
#include "../../include/parser/UpdateASTNode.h"
#include "../../include/parser/IndexASTNode.h"

#include <stdexcept>

namespace Execution {

    std::unique_ptr<IExecutor> ExecutorFactory::createExecutor(
        std::unique_ptr<Parser::ASTNode> ast,
        Catalog::ICatalogManager* catalog,
        Storage::IStorageEngine* storage,
        Integrity::IIntegrityManager* integrity,
        Maintenance::IDatabaseMaintenance* maintenance,
        Security::ISecurityManager* security,
        Index::IIndexManager* index)
    {
        if (!ast) {
            return nullptr;
        }

        // 获取当前节点的具体 SQL 类型进行分发
        switch (ast->getType()) {
            
            // DDL: 修改表
            case Parser::SQLType::ALTER_TABLE:
                return std::make_unique<AlterTableExecutor>(
                    std::unique_ptr<Parser::AlterTableASTNode>(static_cast<Parser::AlterTableASTNode*>(ast.release())),
                    catalog
                );

            // DDL: 删表
            case Parser::SQLType::DROP_TABLE:
                return std::make_unique<DropTableExecutor>(
                    std::unique_ptr<Parser::DropTableASTNode>(static_cast<Parser::DropTableASTNode*>(ast.release())),
                    catalog
                );

            // DDL: 建表
            case Parser::SQLType::CREATE_TABLE:
                return std::make_unique<CreateExecutor>(
                    std::unique_ptr<Parser::CreateTableASTNode>(static_cast<Parser::CreateTableASTNode*>(ast.release())), 
                    catalog, 
                    storage
                );

            // DML: 插入数据 (需要 4 个参数，引入完整性校验)
            case Parser::SQLType::INSERT:
                return std::make_unique<InsertExecutor>(
                    std::unique_ptr<Parser::InsertASTNode>(static_cast<Parser::InsertASTNode*>(ast.release())), 
                    catalog, 
                    storage,
                    integrity,
                    index
                );

            // DQL: 查询数据 (采用流水线/火山模型架构)
            case Parser::SQLType::SELECT:
                return std::make_unique<SelectExecutor>(
                    std::unique_ptr<Parser::SelectASTNode>(static_cast<Parser::SelectASTNode*>(ast.release())), 
                    catalog, 
                    storage,
                    index
                );

            case Parser::SQLType::CREATE_INDEX:
                return std::make_unique<CreateIndexExecutor>(
                    std::unique_ptr<Parser::CreateIndexASTNode>(static_cast<Parser::CreateIndexASTNode*>(ast.release())),
                    catalog,
                    storage,
                    index
                );

            case Parser::SQLType::DROP_INDEX:
                return std::make_unique<DropIndexExecutor>(
                    std::unique_ptr<Parser::DropIndexASTNode>(static_cast<Parser::DropIndexASTNode*>(ast.release())),
                    catalog,
                    storage,
                    index
                );

            // 数据库管理：建库
            case Parser::SQLType::CREATE_DATABASE:
                return std::make_unique<CreateDatabaseExecutor>(
                    std::unique_ptr<Parser::CreateDatabaseASTNode>(static_cast<Parser::CreateDatabaseASTNode*>(ast.release())), 
                    catalog
                );

            // 用户管理
            case Parser::SQLType::CREATE_USER: {
                auto* n = static_cast<Parser::CreateUserASTNode*>(ast.release());
                auto exe = std::make_unique<UserExecutor>(UserExecutor::Op::CREATE_USER, n->username, n->password, "", security);
                return exe;
            }
            case Parser::SQLType::DROP_USER: {
                auto* n = static_cast<Parser::DropUserASTNode*>(ast.release());
                auto exe = std::make_unique<UserExecutor>(UserExecutor::Op::DROP_USER, n->username, "", "", security);
                return exe;
            }
            case Parser::SQLType::GRANT_ROLE: {
                auto* n = static_cast<Parser::GrantRevokeASTNode*>(ast.release());
                auto exe = std::make_unique<UserExecutor>(UserExecutor::Op::GRANT_ROLE, n->username, "", n->role, security);
                return exe;
            }
            case Parser::SQLType::REVOKE_ROLE: {
                auto* n = static_cast<Parser::GrantRevokeASTNode*>(ast.release());
                auto exe = std::make_unique<UserExecutor>(UserExecutor::Op::REVOKE_ROLE, n->username, "", "", security);
                return exe;
            }

            // 数据库管理：备份
            case Parser::SQLType::BACKUP_DATABASE: {
                auto* node = static_cast<Parser::BackupDatabaseASTNode*>(ast.get());
                return std::make_unique<BackupRestoreExecutor>(
                    BackupRestoreExecutor::Mode::BACKUP, node->dbName, node->path, catalog, maintenance);
            }
            // 数据库管理：还原
            case Parser::SQLType::RESTORE_DATABASE: {
                auto* node = static_cast<Parser::RestoreDatabaseASTNode*>(ast.get());
                return std::make_unique<BackupRestoreExecutor>(
                    BackupRestoreExecutor::Mode::RESTORE, node->dbName, node->path, catalog, maintenance);
            }

            // 数据库管理：删库
            case Parser::SQLType::DROP_DATABASE:
                return std::make_unique<DropDatabaseExecutor>(
                    std::unique_ptr<Parser::DropDatabaseASTNode>(static_cast<Parser::DropDatabaseASTNode*>(ast.release())),
                    catalog
                );

            // 数据库管理：切换库
            case Parser::SQLType::USE_DATABASE:
                return std::make_unique<UseDatabaseExecutor>(
                    std::unique_ptr<Parser::UseDatabaseASTNode>(static_cast<Parser::UseDatabaseASTNode*>(ast.release())), 
                    catalog
                );

            // DML: 删除与更新 (目前仅提供占位，后续补充实现)
            case Parser::SQLType::DELETE_:
                return std::make_unique<DeleteExecutor>(
                    std::unique_ptr<Parser::DeleteASTNode>(static_cast<Parser::DeleteASTNode*>(ast.release())), 
                    catalog,
                    storage,
                    integrity,
                    index
                );

            case Parser::SQLType::UPDATE:
                return std::make_unique<UpdateExecutor>(
                    std::unique_ptr<Parser::UpdateASTNode>(static_cast<Parser::UpdateASTNode*>(ast.release())), 
                    catalog,
                    storage,
                    integrity,
                    index
                );

            default:
                throw std::runtime_error("Executor Factory Error: 未定义的 SQL 执行路径。");
        }
    }

} // namespace Execution
