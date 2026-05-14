#include "QueryBuilder.h"
#include "operators/SeqScanOperator.h"
#include "operators/JoinOperator.h"
#include "operators/FilterOperator.h"
#include "operators/AggregateOperator.h"
#include "../../include/parser/SelectASTNode.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
#include <vector>
#include <string>

namespace {

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

std::unique_ptr<IOperator> buildTableSource(Parser::ASTNode* source,
                                             Catalog::ICatalogManager* catalog,
                                             Storage::IStorageEngine* storage) {
    if (!source) return nullptr;

    if (auto table = dynamic_cast<Parser::TableNode*>(source)) {
        return std::make_unique<SeqScanOperator>(table->tableName, catalog, storage);
    }

    if (auto join = dynamic_cast<Parser::JoinNode*>(source)) {
        if (join->joinType == "SUBQUERY") {
            if (auto aliasTable = dynamic_cast<Parser::TableNode*>(join->right.get())) {
                return std::make_unique<SeqScanOperator>(aliasTable->tableName, catalog, storage);
            }
            return nullptr;
        }

        auto leftOp = buildTableSource(join->left.get(), catalog, storage);
        auto rightOp = buildTableSource(join->right.get(), catalog, storage);
        if (!leftOp || !rightOp) return nullptr;

        if (!leftOp->init() || !rightOp->init()) return nullptr;
        auto leftSchema = leftOp->getOutputSchema();
        auto rightSchema = rightOp->getOutputSchema();

        leftOp = buildTableSource(join->left.get(), catalog, storage);
        rightOp = buildTableSource(join->right.get(), catalog, storage);

        return std::make_unique<JoinOperator>(
            std::move(leftOp), std::move(rightOp),
            join->joinType, join->onCondition.get(),
            leftSchema, rightSchema);
    }

    return nullptr;
}

std::vector<std::vector<std::string>> runSubquery(Parser::ASTNode* subqueryRoot,
                                                   Catalog::ICatalogManager* catalog,
                                                   Storage::IStorageEngine* storage,
                                                   const std::vector<std::string>* outerRow,
                                                   const std::vector<Meta::FieldBlock>* outerSchema,
                                                   const std::string* outerAlias) {
    std::vector<std::vector<std::string>> result;

    Parser::SelectASTNode* selNode = nullptr;
    if (auto sub = dynamic_cast<Parser::SubqueryNode*>(subqueryRoot))
        selNode = dynamic_cast<Parser::SelectASTNode*>(sub->subquery.get());
    else
        selNode = dynamic_cast<Parser::SelectASTNode*>(subqueryRoot);

    if (!selNode || !catalog || !storage) return result;

    // 注入外部上下文的辅助函数
    auto injectContext = [&](FilterOperator& f) {
        f.setSubqueryContext(catalog, storage);
        if (outerRow && outerSchema && outerAlias)
            f.setOuterContext(outerRow, *outerSchema, *outerAlias);
    };

    auto rootOp = buildTableSource(selNode->fromSource.get(), catalog, storage);
    if (!rootOp) return result;

    // WHERE
    if (selNode->whereExpressionTree) {
        auto f = std::make_unique<FilterOperator>(std::move(rootOp), selNode->whereExpressionTree.get());
        injectContext(*f);
        rootOp = std::move(f);
    }

    // 检测聚合函数
    bool hasGb = !selNode->groupByFields.empty();
    bool hasAgg = false;
    for (const auto& t : selNode->targetFields) {
        if (dynamic_cast<Parser::FunctionCallNode*>(t.get())) { hasAgg = true; break; }
        if (auto bin = dynamic_cast<Parser::BinaryOperatorNode*>(t.get())) {
            if (bin->op == "AS" && dynamic_cast<Parser::FunctionCallNode*>(bin->left.get())) {
                hasAgg = true; break;
            }
        }
    }

    // HAVING 聚合函数占位节点（必须在 if 块外声明，保证生命周期覆盖 AggregateOperator::init()）
    std::vector<Parser::FunctionCallNode> havingNodes;

    // GROUP BY + 聚合
    if (hasGb || hasAgg) {
        if (!rootOp->init()) return result;
        auto inSchema = rootOp->getOutputSchema();
        rootOp = buildTableSource(selNode->fromSource.get(), catalog, storage);
        if (selNode->whereExpressionTree) {
            auto f = std::make_unique<FilterOperator>(std::move(rootOp), selNode->whereExpressionTree.get());
            injectContext(*f);
            rootOp = std::move(f);
        }
        std::vector<Parser::ASTNode*> gbPtrs, tgtPtrs;
        for (auto& g : selNode->groupByFields) gbPtrs.push_back(g.get());
        for (auto& t : selNode->targetFields) tgtPtrs.push_back(t.get());

        std::vector<std::string> havingNames;
        if (selNode->havingExpressionTree) {
            collectHavingAggs(selNode->havingExpressionTree.get(), havingNames);
            for (const auto& name : havingNames) {
                havingNodes.push_back(Parser::FunctionCallNode());
                havingNodes.back().functionName = name;
                tgtPtrs.push_back(&havingNodes.back());
            }
        }

        rootOp = std::make_unique<AggregateOperator>(std::move(rootOp), gbPtrs, tgtPtrs, inSchema);
        if (selNode->havingExpressionTree) {
            auto f = std::make_unique<FilterOperator>(std::move(rootOp), selNode->havingExpressionTree.get());
            injectContext(*f);
            rootOp = std::move(f);
        }
    }

    if (!rootOp->init()) return result;
    std::vector<std::string> r;
    while (rootOp->next(r)) result.push_back(r);
    return result;
}

} // namespace Execution
