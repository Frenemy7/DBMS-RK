#include "QueryBuilder.h"
#include "operators/SeqScanOperator.h"
#include "operators/IndexScanOperator.h"
#include "IndexMaintenance.h"
#include "operators/JoinOperator.h"
#include "operators/FilterOperator.h"
#include "operators/AggregateOperator.h"
#include "../../include/parser/SelectASTNode.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
#include "../../include/index/IIndexManager.h"
#include <algorithm>
#include <map>
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

std::string stripQualifier(const Parser::ColumnRefNode* col) {
    return col ? col->columnName : "";
}

bool collectEqualityLiterals(Parser::ASTNode* node, std::map<std::string, std::string>& out) {
    if (!node) return true;
    auto bin = dynamic_cast<Parser::BinaryOperatorNode*>(node);
    if (!bin) return true;

    if (bin->op == "AND") {
        return collectEqualityLiterals(bin->left.get(), out) &&
               collectEqualityLiterals(bin->right.get(), out);
    }
    if (bin->op != "=") return true;

    auto leftCol = dynamic_cast<Parser::ColumnRefNode*>(bin->left.get());
    auto rightCol = dynamic_cast<Parser::ColumnRefNode*>(bin->right.get());
    auto leftLit = dynamic_cast<Parser::LiteralNode*>(bin->left.get());
    auto rightLit = dynamic_cast<Parser::LiteralNode*>(bin->right.get());

    if (leftCol && rightLit) {
        out[stripQualifier(leftCol)] = rightLit->value;
    } else if (rightCol && leftLit) {
        out[stripQualifier(rightCol)] = leftLit->value;
    }
    return true;
}

bool buildKeyForIndex(const Meta::IndexBlock& indexMeta,
                      const std::map<std::string, std::string>& equalities,
                      const std::vector<Meta::FieldBlock>& fields,
                      Index::IndexKey& key) {
    if (indexMeta.field_num <= 0 || indexMeta.field_num > 2) return false;
    key.values.clear();
    for (int i = 0; i < indexMeta.field_num; ++i) {
        auto it = equalities.find(indexMeta.fields[i]);
        if (it == equalities.end()) return false;
        auto fieldIt = std::find_if(fields.begin(), fields.end(), [&](const Meta::FieldBlock& field) {
            return std::string(field.name) == indexMeta.fields[i];
        });
        if (fieldIt == fields.end()) return false;
        key.values.push_back(Execution::IndexMaintenance::encodeValueForIndex(it->second, *fieldIt));
    }
    return true;
}

struct RangePredicate {
    bool hasLower = false;
    bool includeLower = true;
    std::string lower;
    bool hasUpper = false;
    bool includeUpper = true;
    std::string upper;
};

bool collectRangePredicates(Parser::ASTNode* node, std::map<std::string, RangePredicate>& out) {
    if (!node) return true;

    if (auto between = dynamic_cast<Parser::BetweenNode*>(node)) {
        auto col = dynamic_cast<Parser::ColumnRefNode*>(between->expr.get());
        auto low = dynamic_cast<Parser::LiteralNode*>(between->low.get());
        auto high = dynamic_cast<Parser::LiteralNode*>(between->high.get());
        if (col && low && high) {
            auto& pred = out[stripQualifier(col)];
            pred.hasLower = true;
            pred.includeLower = true;
            pred.lower = low->value;
            pred.hasUpper = true;
            pred.includeUpper = true;
            pred.upper = high->value;
        }
        return true;
    }

    auto bin = dynamic_cast<Parser::BinaryOperatorNode*>(node);
    if (!bin) return true;
    if (bin->op == "AND") {
        return collectRangePredicates(bin->left.get(), out) &&
               collectRangePredicates(bin->right.get(), out);
    }

    auto leftCol = dynamic_cast<Parser::ColumnRefNode*>(bin->left.get());
    auto rightCol = dynamic_cast<Parser::ColumnRefNode*>(bin->right.get());
    auto leftLit = dynamic_cast<Parser::LiteralNode*>(bin->left.get());
    auto rightLit = dynamic_cast<Parser::LiteralNode*>(bin->right.get());

    if (leftCol && rightLit) {
        auto& pred = out[stripQualifier(leftCol)];
        if (bin->op == ">") { pred.hasLower = true; pred.includeLower = false; pred.lower = rightLit->value; }
        else if (bin->op == ">=") { pred.hasLower = true; pred.includeLower = true; pred.lower = rightLit->value; }
        else if (bin->op == "<") { pred.hasUpper = true; pred.includeUpper = false; pred.upper = rightLit->value; }
        else if (bin->op == "<=") { pred.hasUpper = true; pred.includeUpper = true; pred.upper = rightLit->value; }
    } else if (rightCol && leftLit) {
        auto& pred = out[stripQualifier(rightCol)];
        if (bin->op == "<") { pred.hasLower = true; pred.includeLower = false; pred.lower = leftLit->value; }
        else if (bin->op == "<=") { pred.hasLower = true; pred.includeLower = true; pred.lower = leftLit->value; }
        else if (bin->op == ">") { pred.hasUpper = true; pred.includeUpper = false; pred.upper = leftLit->value; }
        else if (bin->op == ">=") { pred.hasUpper = true; pred.includeUpper = true; pred.upper = leftLit->value; }
    }
    return true;
}

std::unique_ptr<Execution::IOperator> tryBuildIndexScan(Parser::TableNode* table,
                                                        Parser::ASTNode* where,
                                                        Catalog::ICatalogManager* catalog,
                                                        Storage::IStorageEngine* storage,
                                                        Index::IIndexManager* index) {
    if (!table || !where || !catalog || !storage || !index) return nullptr;

    std::map<std::string, std::string> equalities;
    collectEqualityLiterals(where, equalities);

    auto indices = index->getIndices(catalog->getCurrentDatabase(), table->tableName);
    auto fields = Execution::IndexMaintenance::sortedFields(catalog, table->tableName);
    if (!equalities.empty()) {
        for (const auto& indexMeta : indices) {
            Index::IndexKey key;
            if (buildKeyForIndex(indexMeta, equalities, fields, key)) {
                return std::make_unique<Execution::IndexScanOperator>(
                    table->tableName, indexMeta, key, catalog, storage, index);
            }
        }
    }

    std::map<std::string, RangePredicate> ranges;
    collectRangePredicates(where, ranges);
    for (const auto& indexMeta : indices) {
        if (indexMeta.field_num != 1) continue;
        auto it = ranges.find(indexMeta.fields[0]);
        if (it == ranges.end()) continue;
        auto fieldIt = std::find_if(fields.begin(), fields.end(), [&](const Meta::FieldBlock& field) {
            return std::string(field.name) == indexMeta.fields[0];
        });
        if (fieldIt == fields.end()) continue;

        const auto& pred = it->second;
        if (!pred.hasLower && !pred.hasUpper) continue;

        Index::IndexKey lower({pred.hasLower
            ? Execution::IndexMaintenance::encodeValueForIndex(pred.lower, *fieldIt)
            : std::string()});
        Index::IndexKey upper({pred.hasUpper
            ? Execution::IndexMaintenance::encodeValueForIndex(pred.upper, *fieldIt)
            : std::string(127, static_cast<char>(0xFF))});
        return std::make_unique<Execution::IndexScanOperator>(
            table->tableName,
            indexMeta,
            lower,
            upper,
            pred.hasLower ? pred.includeLower : true,
            pred.hasUpper ? pred.includeUpper : true,
            catalog,
            storage,
            index);
    }
    return nullptr;
}

} // namespace

namespace Execution {

std::unique_ptr<IOperator> buildTableSource(Parser::ASTNode* source,
                                             Catalog::ICatalogManager* catalog,
                                             Storage::IStorageEngine* storage,
                                             Index::IIndexManager* index,
                                             Parser::ASTNode* where) {
    if (!source) return nullptr;

    if (auto table = dynamic_cast<Parser::TableNode*>(source)) {
        if (auto indexScan = tryBuildIndexScan(table, where, catalog, storage, index)) {
            return indexScan;
        }
        return std::make_unique<SeqScanOperator>(table->tableName, catalog, storage);
    }

    if (auto join = dynamic_cast<Parser::JoinNode*>(source)) {
        if (join->joinType == "SUBQUERY") {
            if (auto aliasTable = dynamic_cast<Parser::TableNode*>(join->right.get())) {
                return std::make_unique<SeqScanOperator>(aliasTable->tableName, catalog, storage);
            }
            return nullptr;
        }

        auto leftOp = buildTableSource(join->left.get(), catalog, storage, index, nullptr);
        auto rightOp = buildTableSource(join->right.get(), catalog, storage, index, nullptr);
        if (!leftOp || !rightOp) return nullptr;

        if (!leftOp->init() || !rightOp->init()) return nullptr;
        auto leftSchema = leftOp->getOutputSchema();
        auto rightSchema = rightOp->getOutputSchema();

        leftOp = buildTableSource(join->left.get(), catalog, storage, index, nullptr);
        rightOp = buildTableSource(join->right.get(), catalog, storage, index, nullptr);

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
