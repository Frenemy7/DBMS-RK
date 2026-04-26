#include "ExecutorFactory.h"
#include "CreateExecutor.h"
#include "InsertExecutor.h"
#include "SelectExecutor.h"
#include "DeleteExecutor.h"
#include "UpdateExecutor.h"
#include "CreateDatabaseExecutor.h"
#include "UseDatabaseExecutor.h"

// 引入具体的 AST 子类定义以便进行 static_cast
#include "../../include/parser/CreateTableASTNode.h"
#include "../../include/parser/InsertASTNode.h"
#include "../../include/parser/SelectASTNode.h"
#include "../../include/parser/DeleteASTNode.h"
#include "../../include/parser/UpdateASTNode.h"

#include <stdexcept>

namespace Execution {

    std::unique_ptr<IExecutor> ExecutorFactory::createExecutor(
        std::unique_ptr<Parser::ASTNode> ast, 
        Catalog::ICatalogManager* catalog, 
        Storage::IStorageEngine* storage,
        Integrity::IIntegrityManager* integrity) 
    {
        if (!ast) {
            return nullptr;
        }

        // 获取当前节点的具体 SQL 类型进行分发
        switch (ast->getType()) {
            
            // 1. DDL: 建表
            case Parser::SQLType::CREATE_TABLE:
                return std::make_unique<CreateExecutor>(
                    std::unique_ptr<Parser::CreateTableASTNode>(static_cast<Parser::CreateTableASTNode*>(ast.release())), 
                    catalog, 
                    storage
                );

            // 2. DML: 插入数据 (需要 4 个参数，引入完整性校验)
            case Parser::SQLType::INSERT:
                return std::make_unique<InsertExecutor>(
                    std::unique_ptr<Parser::InsertASTNode>(static_cast<Parser::InsertASTNode*>(ast.release())), 
                    catalog, 
                    storage,
                    integrity
                );

            // 3. DQL: 查询数据 (采用流水线/火山模型架构)
            case Parser::SQLType::SELECT:
                return std::make_unique<SelectExecutor>(
                    std::unique_ptr<Parser::SelectASTNode>(static_cast<Parser::SelectASTNode*>(ast.release())), 
                    catalog, 
                    storage
                );

            // 4. 数据库管理：建库
            case Parser::SQLType::CREATE_DATABASE:
                return std::make_unique<CreateDatabaseExecutor>(
                    std::unique_ptr<Parser::CreateDatabaseASTNode>(static_cast<Parser::CreateDatabaseASTNode*>(ast.release())), 
                    catalog
                );

            // 5. 数据库管理：切换库
            case Parser::SQLType::USE_DATABASE:
                return std::make_unique<UseDatabaseExecutor>(
                    std::unique_ptr<Parser::UseDatabaseASTNode>(static_cast<Parser::UseDatabaseASTNode*>(ast.release())), 
                    catalog
                );

            // 6. DML: 删除与更新 (目前仅提供占位，后续补充实现)
            case Parser::SQLType::DELETE_:
                return std::make_unique<DeleteExecutor>(
                    std::unique_ptr<Parser::DeleteASTNode>(static_cast<Parser::DeleteASTNode*>(ast.release())), 
                    catalog,
                    storage,
                    integrity
                );

            case Parser::SQLType::UPDATE:
                return std::make_unique<UpdateExecutor>(
                    std::unique_ptr<Parser::UpdateASTNode>(static_cast<Parser::UpdateASTNode*>(ast.release())), 
                    catalog,
                    storage,
                    integrity
                );

            default:
                throw std::runtime_error("Executor Factory Error: 未定义的 SQL 执行路径。");
        }
    }

} // namespace Execution