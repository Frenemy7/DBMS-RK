#include "SelectExecutor.h"
#include "operators/SeqScanOperator.h"
#include "operators/FilterOperator.h"
#include "operators/ProjectOperator.h"
#include <iostream>
#include <iomanip>

namespace Execution {

    SelectExecutor::SelectExecutor(std::unique_ptr<Parser::SelectASTNode> ast,
                                   Catalog::ICatalogManager* catalog,
                                   Storage::IStorageEngine* storage)
        : astNode(std::move(ast)), catalogManager(catalog), storageEngine(storage) {}

    bool SelectExecutor::execute() {
        if (!astNode || !catalogManager || !storageEngine) {
            std::cerr << "[执行错误] SelectExecutor 依赖缺失！" << std::endl;
            return false;
        }

        std::unique_ptr<IOperator> rootOperator = nullptr;

        // 解析 AST，自底向上拼装
        // 解析 FROM 子句 (生成 SeqScan 底层扫描车间)
        if (auto tableNode = dynamic_cast<Parser::TableNode*>(astNode->fromSource.get())) {
            rootOperator = std::make_unique<SeqScanOperator>(tableNode->tableName, catalogManager, storageEngine);
        } else {
            std::cerr << "Error: FROM 子句目前仅支持单物理表扫描。" << std::endl;
            return false;
        }

        // 解析 WHERE 子句 (生成 Filter 质检车间，包裹在 Scan 外面)
        if (astNode->whereExpressionTree != nullptr) {
            rootOperator = std::make_unique<FilterOperator>(
                std::move(rootOperator), 
                astNode->whereExpressionTree.get()
            );
        }

        // 解析 SELECT 目标列 (生成 Project 裁剪车间，包裹在最外面)
        std::vector<std::string> targetCols;
        for (const auto& targetNode : astNode->targetFields) {
            // 解析 AST 中的 ColumnRefNode
            if (auto colRef = dynamic_cast<Parser::ColumnRefNode*>(targetNode.get())) {
                targetCols.push_back(colRef->columnName);
            }
        }
        
        // 挂载投影车间 (如果 targetCols 为空，ProjectOperator 内部会自动按 SELECT * 处理)
        rootOperator = std::make_unique<ProjectOperator>(
            std::move(rootOperator), 
            targetCols
        );

        // 初始化整个流水线
        if (!rootOperator->init()) {
            // 如果报错（比如表不存在，列名写错），子算子已经打印了 Error，这里直接退出
            return false; 
        }
        // 获取流水线顶层吐出的最终表头
        std::vector<Meta::FieldBlock> finalSchema = rootOperator->getOutputSchema();

        // 打印表头边界线
        std::cout << "+" << std::string(finalSchema.size() * 16 - 1, '-') << "+" << std::endl;
        
        // 打印列名
        for (const auto& field : finalSchema) {
            std::cout << "| " << std::left << std::setw(14) << field.name;
        }
        std::cout << "|" << std::endl;
        std::cout << "+" << std::string(finalSchema.size() * 16 - 1, '-') << "+" << std::endl;

        // 循环拉取数据
        std::vector<std::string> row;
        int rowCount = 0;
        
        while (rootOperator->next(row)) {
            for (const auto& col : row) {
                // 如果字符串过长，截断以保持表格美观
                std::string displayCol = col.length() > 13 ? col.substr(0, 10) + "..." : col;
                std::cout << "| " << std::left << std::setw(14) << displayCol;
            }
            std::cout << "|" << std::endl;
            rowCount++;
        }

        // 打印底部统计
        std::cout << "+" << std::string(finalSchema.size() * 16 - 1, '-') << "+" << std::endl;
        std::cout << rowCount << " rows in set." << std::endl;

        return true;
    }

} // namespace Execution