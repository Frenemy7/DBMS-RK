#include "FilterOperator.h"
#include "../QueryBuilder.h"
#include "../../../include/parser/SelectASTNode.h"
#include "../../../include/catalog/ICatalogManager.h"
#include "../../../include/storage/IStorageEngine.h"
#include <cmath>
#include <iostream>
#include <algorithm>

namespace Execution {

    FilterOperator::FilterOperator(std::unique_ptr<IOperator> child, Parser::ASTNode* condition)
        : child_(std::move(child)), conditionTree_(condition) {}

    void FilterOperator::setSubqueryContext(Catalog::ICatalogManager* catalog, Storage::IStorageEngine* storage) {
        catalog_ = catalog;
        storage_ = storage;
    }

    void FilterOperator::setOuterContext(const std::vector<std::string>* outerRow,
                                          const std::vector<Meta::FieldBlock>& outerSchema,
                                          const std::string& outerAlias) {
        outerRow_ = outerRow;
        outerSchema_ = outerSchema;
        outerAlias_ = outerAlias;
    }

    // 从外部行取值（关联子查询用）
    std::string FilterOperator::getValueFromOuter(const std::string& colName) const {
        if (!outerRow_) return "";
        for (size_t i = 0; i < outerSchema_.size(); ++i) {
            if (std::string(outerSchema_[i].name) == colName) {
                if (i < outerRow_->size()) return (*outerRow_)[i];
            }
        }
        return "";
    }

    // 执行子查询：外部当前行通过参数传入，不会被子查询内部覆盖
    std::vector<std::vector<std::string>> FilterOperator::executeSubquery(
        Parser::ASTNode* subqueryRoot, const std::vector<std::string>& outerRow) {
        return runSubquery(subqueryRoot, catalog_, storage_, &outerRow, &outerSchema_, &outerAlias_);
    }

    bool FilterOperator::init() {
        if (!child_->init()) return false;
        childSchema_ = child_->getOutputSchema();
        return true;
    }

    bool FilterOperator::next(std::vector<std::string>& row) {
        while (child_->next(row)) {
            if (evaluate(row, conditionTree_)) {
                return true;
            }
        }
        return false;
    }

    std::vector<Meta::FieldBlock> FilterOperator::getOutputSchema() const {
        return childSchema_;
    }

    // 从当前行数组中按列名取数据
    std::string FilterOperator::getValueFromRow(const std::vector<std::string>& row, const std::string& colName) {
        for (size_t i = 0; i < childSchema_.size(); ++i) {
            if (std::string(childSchema_[i].name) == colName) {
                if (i < row.size()) return row[i];
            }
        }
        return "";
    }

    // AST 节点求值：列引用、字面量、聚合函数引用、算术运算
    std::string FilterOperator::evaluateValue(const std::vector<std::string>& row, Parser::ASTNode* node) {
        if (!node) return "";

        if (auto col = dynamic_cast<Parser::ColumnRefNode*>(node)) {
            // 有表前缀 → 判断是外部关联列还是当前列
            if (!col->tableName.empty() && !outerAlias_.empty()) {
                if (col->tableName == outerAlias_) {
                    return getValueFromOuter(col->columnName);
                }
            }
            return getValueFromRow(row, col->columnName);
        }
        if (auto lit = dynamic_cast<Parser::LiteralNode*>(node)) {
            return lit->value;
        }
        // 聚合函数引用（HAVING 阶段已计算，函数名就是列名）
        if (auto func = dynamic_cast<Parser::FunctionCallNode*>(node)) {
            return getValueFromRow(row, func->functionName);
        }
        // 算术运算：+、-、*、/
        if (auto bin = dynamic_cast<Parser::BinaryOperatorNode*>(node)) {
            if (bin->op == "+" || bin->op == "-" || bin->op == "*" || bin->op == "/") {
                std::string lv = evaluateValue(row, bin->left.get());
                std::string rv = evaluateValue(row, bin->right.get());
                try {
                    double ln = std::stod(lv), rn = std::stod(rv);
                    if (bin->op == "+") return std::to_string(ln + rn);
                    if (bin->op == "-") return std::to_string(ln - rn);
                    if (bin->op == "*") return std::to_string(ln * rn);
                    if (bin->op == "/") return rn != 0 ? std::to_string(ln / rn) : "0";
                } catch (...) { return "0"; }
            }
        }
        return "";
    }

    // 递归求值：计算表达式的布尔值（WHERE / ON / HAVING 条件）
    bool FilterOperator::evaluate(const std::vector<std::string>& row, Parser::ASTNode* node) {
        if (!node) return true;

        if (auto bin = dynamic_cast<Parser::BinaryOperatorNode*>(node)) {
            // 逻辑组合
            if (bin->op == "AND") return evaluate(row, bin->left.get()) && evaluate(row, bin->right.get());
            if (bin->op == "OR")  return evaluate(row, bin->left.get()) || evaluate(row, bin->right.get());
            if (bin->op == "NOT") return !evaluate(row, bin->left.get());

            // EXISTS：把当前行作为外部上下文传入子查询
            if (bin->op == "EXISTS") {
                auto results = executeSubquery(bin->left.get(), row);
                return !results.empty();
            }

            // IN / NOT IN（子查询或值列表）
            if (bin->op == "IN" || bin->op == "NOT IN") {
                std::string leftVal = evaluateValue(row, bin->left.get());

                // 子查询
                if (dynamic_cast<Parser::SubqueryNode*>(bin->right.get()) ||
                    dynamic_cast<Parser::SelectASTNode*>(bin->right.get())) {
                    auto results = executeSubquery(bin->right.get(), row);
                    bool found = false;
                    for (const auto& r : results) {
                        if (!r.empty() && r[0] == leftVal) { found = true; break; }
                    }
                    return bin->op == "IN" ? found : !found;
                }

                // IN 值列表
                if (auto inList = dynamic_cast<Parser::InListNode*>(bin->right.get())) {
                    bool found = false;
                    for (const auto& item : inList->items) {
                        auto lit = dynamic_cast<Parser::LiteralNode*>(item.get());
                        if (lit) {
                            std::string v = lit->value;
                            if (v.size() >= 2 && v.front() == '\'' && v.back() == '\'')
                                v = v.substr(1, v.size() - 2);
                            if (v == leftVal) { found = true; break; }
                        }
                    }
                    return bin->op == "IN" ? found : !found;
                }
                return false;
            }

            // 标量子查询比较：x > (SELECT ...), x = (SELECT ...) 等
            std::string leftVal = evaluateValue(row, bin->left.get());
            std::string rightVal;

            if (dynamic_cast<Parser::SubqueryNode*>(bin->right.get())) {
                auto results = executeSubquery(bin->right.get(), row);
                if (!results.empty() && !results[0].empty()) rightVal = results[0][0];
            } else if (dynamic_cast<Parser::SelectASTNode*>(bin->right.get())) {
                auto results = executeSubquery(bin->right.get(), row);
                if (!results.empty() && !results[0].empty()) rightVal = results[0][0];
            } else {
                rightVal = evaluateValue(row, bin->right.get());
            }

            // 数值比较
            try {
                if (!leftVal.empty() && !rightVal.empty()) {
                    double ln = std::stod(leftVal), rn = std::stod(rightVal);
                    if (bin->op == "=")  return std::fabs(ln - rn) < 1e-9;
                    if (bin->op == ">")  return ln > rn;
                    if (bin->op == "<")  return ln < rn;
                    if (bin->op == ">=") return ln >= rn;
                    if (bin->op == "<=") return ln <= rn;
                    if (bin->op == "!=" || bin->op == "<>") return std::fabs(ln - rn) >= 1e-9;
                }
            } catch (...) {}

            // 字符串比较
            if (bin->op == "=")  return leftVal == rightVal;
            if (bin->op == ">")  return leftVal > rightVal;
            if (bin->op == "<")  return leftVal < rightVal;
            if (bin->op == ">=") return leftVal >= rightVal;
            if (bin->op == "<=") return leftVal <= rightVal;
            if (bin->op == "!=" || bin->op == "<>") return leftVal != rightVal;

            // LIKE 模糊匹配
            if (bin->op == "LIKE") {
                std::string pat = rightVal;
                if (pat.size() >= 2 && pat.front() == '%' && pat.back() == '%')
                    return leftVal.find(pat.substr(1, pat.size() - 2)) != std::string::npos;
                if (!pat.empty() && pat.front() == '%')
                    return leftVal.size() >= pat.size() - 1 &&
                           leftVal.substr(leftVal.size() - pat.size() + 1) == pat.substr(1);
                if (!pat.empty() && pat.back() == '%')
                    return leftVal.substr(0, pat.size() - 1) == pat.substr(0, pat.size() - 1);
                return leftVal == rightVal;
            }
        }

        // IS NULL / IS NOT NULL
        if (auto isNull = dynamic_cast<Parser::IsNullNode*>(node)) {
            std::string val = evaluateValue(row, isNull->expr.get());
            bool empty = val.empty() || val == "NULL" || val == "null";
            return isNull->notNull ? !empty : empty;
        }

        // BETWEEN ... AND ...
        if (auto between = dynamic_cast<Parser::BetweenNode*>(node)) {
            std::string ev = evaluateValue(row, between->expr.get());
            std::string lv = evaluateValue(row, between->low.get());
            std::string hv = evaluateValue(row, between->high.get());
            try {
                double de = std::stod(ev), dl = std::stod(lv), dh = std::stod(hv);
                return de >= dl && de <= dh;
            } catch (...) {}
            return ev >= lv && ev <= hv;
        }

        // 独立的布尔字面量（用于 HAVING 等场景）
        if (auto lit = dynamic_cast<Parser::LiteralNode*>(node)) {
            return !lit->value.empty() && lit->value != "0";
        }

        return false;
    }

} // namespace Execution
