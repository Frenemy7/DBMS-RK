#include "SelectExecutor.h"
#include "QueryBuilder.h"
#include "../index/IndexManagerImpl.h"
#include "operators/SeqScanOperator.h"
#include "operators/FilterOperator.h"
#include "operators/ProjectOperator.h"
#include "operators/JoinOperator.h"
#include "operators/AggregateOperator.h"
#include "operators/SortOperator.h"
#include "../../include/parser/SelectASTNode.h"
#include <algorithm>
#include <iostream>
#include <iomanip>

namespace {

// 从 HAVING AST 中收集聚合函数名，AggregateOperator 需要计算这些列
void collectHavingAggs(Parser::ASTNode* node, std::vector<std::string>& out) {
    if (!node) return;
    if (auto bin = dynamic_cast<Parser::BinaryOperatorNode*>(node)) {
        collectHavingAggs(bin->left.get(), out);
        collectHavingAggs(bin->right.get(), out);
    }
    if (auto func = dynamic_cast<Parser::FunctionCallNode*>(node)) {
        out.push_back(func->functionName);
    }
}

} // namespace

namespace Execution {

SelectExecutor::SelectExecutor(std::unique_ptr<Parser::SelectASTNode> ast,
                               Catalog::ICatalogManager* catalog,
                               Storage::IStorageEngine* storage,
                               Index::IIndexManager* index)
    : astNode(std::move(ast)), catalogManager(catalog), storageEngine(storage), indexManager_(index) {}

Index::IIndexManager* SelectExecutor::indexManager() {
    if (indexManager_) return indexManager_;
    if (!ownedIndexManager_) {
        ownedIndexManager_ = std::make_unique<Index::IndexManagerImpl>(storageEngine);
    }
    return ownedIndexManager_.get();
}

bool SelectExecutor::execute() {
    if (!astNode || !catalogManager || !storageEngine) {
        std::cerr << "[执行错误] SelectExecutor 依赖缺失！" << std::endl;
        return false;
    }

    // 1. 从 FROM/JOIN 构建底层算子
    Index::IIndexManager* manager = indexManager();

    std::unique_ptr<IOperator> rootOp = buildTableSource(
        astNode->fromSource.get(),
        catalogManager,
        storageEngine,
        manager,
        astNode->whereExpressionTree.get());
    if (!rootOp) {
        std::cerr << "Error: 无法解析 FROM 子句。" << std::endl;
        return false;
    }

    // 提取主查询的别名和 schema（用于关联子查询）
    std::string mainAlias;
    std::vector<Meta::FieldBlock> mainSchema;
    if (auto tableNode = dynamic_cast<Parser::TableNode*>(astNode->fromSource.get())) {
        mainAlias = tableNode->alias.empty() ? tableNode->tableName : tableNode->alias;
        if (catalogManager->hasTable(tableNode->tableName)) {
            mainSchema = catalogManager->getFields(tableNode->tableName);
            std::sort(mainSchema.begin(), mainSchema.end(),
                [](const Meta::FieldBlock& a, const Meta::FieldBlock& b) { return a.order < b.order; });
        }
    }

    // 2. WHERE → Filter
    if (astNode->whereExpressionTree) {
        auto f = std::make_unique<FilterOperator>(std::move(rootOp), astNode->whereExpressionTree.get());
        f->setSubqueryContext(catalogManager, storageEngine);
        f->setOuterContext(nullptr, mainSchema, mainAlias);
        rootOp = std::move(f);
    }

    // 3. GROUP BY / 聚合 → Aggregate
    bool hasGroupBy = !astNode->groupByFields.empty();
    bool hasAggregates = false;
    for (const auto& t : astNode->targetFields) {
        if (dynamic_cast<Parser::FunctionCallNode*>(t.get())) { hasAggregates = true; break; }
        if (auto bin = dynamic_cast<Parser::BinaryOperatorNode*>(t.get())) {
            if (bin->op == "AS" && dynamic_cast<Parser::FunctionCallNode*>(bin->left.get())) {
                hasAggregates = true; break;
            }
        }
    }

    // HAVING 聚合占位节点：必须在 if 块外声明
    std::vector<Parser::FunctionCallNode> havingNodes1;
    std::vector<Parser::FunctionCallNode> havingNodes2;

    if (hasGroupBy || hasAggregates) {
        if (!rootOp->init()) return false;
        auto inSchema = rootOp->getOutputSchema();

        rootOp = buildTableSource(astNode->fromSource.get(), catalogManager, storageEngine, manager, astNode->whereExpressionTree.get());
        if (astNode->whereExpressionTree) {
            auto f = std::make_unique<FilterOperator>(std::move(rootOp), astNode->whereExpressionTree.get());
            f->setSubqueryContext(catalogManager, storageEngine);
            rootOp = std::move(f);
        }

        std::vector<Parser::ASTNode*> groupByPtrs;
        for (auto& g : astNode->groupByFields) groupByPtrs.push_back(g.get());
        std::vector<Parser::ASTNode*> targetPtrs;
        for (auto& t : astNode->targetFields) targetPtrs.push_back(t.get());

        // HAVING 中引用的聚合函数也要计算
        std::vector<std::string> havingNames1a;
        if (astNode->havingExpressionTree) {
            collectHavingAggs(astNode->havingExpressionTree.get(), havingNames1a);
            for (const auto& name : havingNames1a) {
                havingNodes1.push_back(Parser::FunctionCallNode());
                havingNodes1.back().functionName = name;
                targetPtrs.push_back(&havingNodes1.back());
            }
        }

        rootOp = std::make_unique<AggregateOperator>(std::move(rootOp), groupByPtrs, targetPtrs, inSchema);

        if (astNode->havingExpressionTree) {
            auto f = std::make_unique<FilterOperator>(std::move(rootOp), astNode->havingExpressionTree.get());
            f->setSubqueryContext(catalogManager, storageEngine);
            rootOp = std::move(f);
        }
    }

    // 4. ORDER BY → Sort
    if (!astNode->orderByItems.empty()) {
        if (!rootOp->init()) return false;
        auto inSchema = rootOp->getOutputSchema();

        rootOp = buildTableSource(astNode->fromSource.get(), catalogManager, storageEngine, manager, astNode->whereExpressionTree.get());
        if (astNode->whereExpressionTree) {
            auto f = std::make_unique<FilterOperator>(std::move(rootOp), astNode->whereExpressionTree.get());
            f->setSubqueryContext(catalogManager, storageEngine);
            rootOp = std::move(f);
        }
        if (hasGroupBy || hasAggregates) {
            std::vector<Parser::ASTNode*> groupByPtrs;
            for (auto& g : astNode->groupByFields) groupByPtrs.push_back(g.get());
            std::vector<Parser::ASTNode*> targetPtrs;
            for (auto& t : astNode->targetFields) targetPtrs.push_back(t.get());
            std::vector<std::string> havingNames2a;
            if (astNode->havingExpressionTree) {
                collectHavingAggs(astNode->havingExpressionTree.get(), havingNames2a);
                for (const auto& name : havingNames2a) {
                    havingNodes2.push_back(Parser::FunctionCallNode());
                    havingNodes2.back().functionName = name;
                    targetPtrs.push_back(&havingNodes2.back());
                }
            }
            rootOp = std::make_unique<AggregateOperator>(std::move(rootOp), groupByPtrs, targetPtrs, inSchema);
            if (astNode->havingExpressionTree) {
                auto f = std::make_unique<FilterOperator>(std::move(rootOp), astNode->havingExpressionTree.get());
                f->setSubqueryContext(catalogManager, storageEngine);
                rootOp = std::move(f);
            }
        }

        rootOp = std::make_unique<SortOperator>(std::move(rootOp), astNode->orderByItems, inSchema);
    }

    // 5. SELECT target list → Project
    std::vector<std::string> targetCols;
    std::vector<Parser::ASTNode*> targetASTs; // 原始 AST，供 ProjectOperator 求值用
    bool hasStar = false;
    for (auto& t : astNode->targetFields) {
        targetASTs.push_back(t.get());
        if (auto bin = dynamic_cast<Parser::BinaryOperatorNode*>(t.get())) {
            if (bin->op == "AS" && bin->right) {
                if (auto aliasCol = dynamic_cast<Parser::ColumnRefNode*>(bin->right.get())) {
                    targetCols.push_back(aliasCol->columnName);
                    continue;
                }
            }
        }
        if (auto col = dynamic_cast<Parser::ColumnRefNode*>(t.get())) {
            if (col->columnName == "*") { hasStar = true; break; }
            targetCols.push_back(col->columnName);
        }
        else if (auto func = dynamic_cast<Parser::FunctionCallNode*>(t.get())) {
            targetCols.push_back(func->functionName);
        }
    }
    if (hasStar) targetCols.clear();

    rootOp = std::make_unique<ProjectOperator>(std::move(rootOp), targetCols, targetASTs);

    // 6. 初始化并输出
    if (!rootOp->init()) return false;

    std::vector<Meta::FieldBlock> finalSchema = rootOp->getOutputSchema();
    if (finalSchema.empty()) {
        std::cout << "Empty set." << std::endl;
        return true;
    }

    int colWidth = 16;
    std::cout << "+" << std::string(finalSchema.size() * colWidth - 1, '-') << "+" << std::endl;
    for (const auto& field : finalSchema) {
        std::cout << "| " << std::left << std::setw(colWidth - 2) << field.name;
    }
    std::cout << "|" << std::endl;
    std::cout << "+" << std::string(finalSchema.size() * colWidth - 1, '-') << "+" << std::endl;

    std::vector<std::string> row;
    int rowCount = 0;
    while (rootOp->next(row)) {
        for (const auto& col : row) {
            std::string displayCol = col.length() > (size_t)(colWidth - 3) ? col.substr(0, colWidth - 5) + "..." : col;
            std::cout << "| " << std::left << std::setw(colWidth - 2) << displayCol;
        }
        std::cout << "|" << std::endl;
        rowCount++;
    }

    std::cout << "+" << std::string(finalSchema.size() * colWidth - 1, '-') << "+" << std::endl;
    std::cout << rowCount << " rows in set." << std::endl;

    return true;
}

} // namespace Execution
