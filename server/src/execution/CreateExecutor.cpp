#include "CreateExecutor.h"
#include "../../include/common/Constants.h"
#include "../../include/meta/TableMeta.h" 
#include "../../include/integrity/IIntegrityManager.h"
#include <iostream>
#include <cstring>
#include <string>

namespace Execution {

    CreateExecutor::CreateExecutor(std::unique_ptr<Parser::CreateTableASTNode> astNode, 
                                   Catalog::ICatalogManager* catalog, 
                                   Storage::IStorageEngine* storage)
        : astNode(std::move(astNode)), catalogManager(catalog), storageEngine(storage) {}

    bool CreateExecutor::execute() {
        if (!astNode || !catalogManager) {
            std::cerr << "[执行错误] 缺乏 AST 节点或 Catalog 依赖。" << std::endl;
            return false;
        }

        // 表级元数据 (TableBlock)
        Meta::TableBlock tableMeta;
        std::memset(&tableMeta, 0, sizeof(Meta::TableBlock)); // 物理清零，防脏数据

        std::strncpy(tableMeta.name, astNode->tableName.c_str(), Common::MAX_NAME_LEN - 1);
        tableMeta.field_num = astNode->columns.size();
        tableMeta.record_num = 0; 

        // 文件后缀名
        std::strncpy(tableMeta.tdf, (astNode->tableName + ".tdf").c_str(), Common::MAX_PATH_LEN - 1);
        std::strncpy(tableMeta.trd, (astNode->tableName + ".trd").c_str(), Common::MAX_PATH_LEN - 1);
        std::strncpy(tableMeta.tic, (astNode->tableName + ".tic").c_str(), Common::MAX_PATH_LEN - 1);
        std::strncpy(tableMeta.tid, (astNode->tableName + ".tid").c_str(), Common::MAX_PATH_LEN - 1);

        // 字段元数据 (FieldBlock)
        std::vector<Meta::FieldBlock> fields;
        int currentOrder = 0;

        for (const auto& colDef : astNode->columns) { // 列
            Meta::FieldBlock field;
            std::memset(&field, 0, sizeof(Meta::FieldBlock));

            std::strncpy(field.name, colDef.name.c_str(), Common::MAX_NAME_LEN - 1); // 拷贝列名
            field.order = currentOrder++;

            if (colDef.type == "INT") {
                field.type = static_cast<int>(Common::DataType::INTEGER); 
                field.param = 4; 
            } 
            else if (colDef.type == "CHAR" || colDef.type == "VARCHAR") {
                field.type = static_cast<int>(Common::DataType::VARCHAR);
                // 如果 AST 节点中记录了参数 (如 VARCHAR(64))，则填入 param；否则默认 256
                field.param = (colDef.param > 0) ? colDef.param : 256; 
            } 
            else {
                std::cerr << "执行错误：不支持的数据类型 '" << colDef.type << "'" << std::endl;
                return false;
            }

            field.integrities = 0; 

            fields.push_back(field);
        }


        
        // Catalog 管理文件 (创建 .tb, .tdf, 空的 .tic 等文件)
        bool success = catalogManager->createTable(tableMeta, fields);

        if (success) {
            std::cout << "Query OK. 表 '" << astNode->tableName << "' 基础文件创建成功。" << std::endl;
            
            // 处理并写入完整性约束 (IntegrityBlock)
            // 表文件建好后，遍历提取 AST 中的约束，单独写入 .tic 文件
            for (const auto& colDef : astNode->columns) {
                
                // 提取主键约束
                if (colDef.isPrimaryKey) {
                    Meta::IntegrityBlock pkBlock;
                    std::memset(&pkBlock, 0, sizeof(Meta::IntegrityBlock));
                    
                    std::string pkName = "PK_" + astNode->tableName + "_" + colDef.name;
                    std::strncpy(pkBlock.name, pkName.c_str(), Common::MAX_NAME_LEN - 1);
                    std::strncpy(pkBlock.field, colDef.name.c_str(), Common::MAX_NAME_LEN - 1);
                    
                    pkBlock.type = Integrity::PRIMARY_KEY;
                    
                    catalogManager->addIntegrity(astNode->tableName, pkBlock);
                }

                // 提取非空约束
                if (colDef.isNotNull) {
                    Meta::IntegrityBlock nnBlock;
                    std::memset(&nnBlock, 0, sizeof(Meta::IntegrityBlock));
                    
                    std::string nnName = "NN_" + astNode->tableName + "_" + colDef.name;
                    std::strncpy(nnBlock.name, nnName.c_str(), Common::MAX_NAME_LEN - 1);
                    std::strncpy(nnBlock.field, colDef.name.c_str(), Common::MAX_NAME_LEN - 1);
                    
                    nnBlock.type = Integrity::NOT_NULL;
                    
                    catalogManager->addIntegrity(astNode->tableName, nnBlock);
                }

                // 提取唯一约束
                if (colDef.isUnique) {
                    Meta::IntegrityBlock uqBlock;
                    std::memset(&uqBlock, 0, sizeof(Meta::IntegrityBlock));
                    
                    std::string uqName = "UQ_" + astNode->tableName + "_" + colDef.name;
                    std::strncpy(uqBlock.name, uqName.c_str(), Common::MAX_NAME_LEN - 1);
                    std::strncpy(uqBlock.field, colDef.name.c_str(), Common::MAX_NAME_LEN - 1);
                    
                    uqBlock.type = Integrity::UNIQUE;
                    
                    catalogManager->addIntegrity(astNode->tableName, uqBlock);
                }
            }

        } else {
            std::cerr << "Error: 表 '" << astNode->tableName << "' 创建失败。" << std::endl;
        }

        return success;
    }

} // namespace Execution