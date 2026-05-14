#include "ProjectOperator.h"
#include <cmath>
#include <cstring>
#include <iostream>

namespace Execution {

ProjectOperator::ProjectOperator(std::unique_ptr<IOperator> child,
                                  const std::vector<std::string>& targets,
                                  const std::vector<Parser::ASTNode*>& astTargets)
    : child_(std::move(child)), targetFields_(targets), targetASTs_(astTargets) {}

int ProjectOperator::findCol(const std::string& name) const {
    auto schema = child_->getOutputSchema();
    for (size_t i = 0; i < schema.size(); ++i) {
        if (std::string(schema[i].name) == name) return static_cast<int>(i);
    }
    return -1;
}

// 从 AST 节点求值（支持列引用、字面量、算术运算）
std::string ProjectOperator::evalExpr(Parser::ASTNode* node, const std::vector<std::string>& row) {
    if (!node) return "";

    // 别名包装: "AS" 节点 → 对左子树求值
    if (auto bin = dynamic_cast<Parser::BinaryOperatorNode*>(node)) {
        if (bin->op == "AS") return evalExpr(bin->left.get(), row);
    }

    // 列引用
    if (auto col = dynamic_cast<Parser::ColumnRefNode*>(node)) {
        int idx = findCol(col->columnName);
        if (idx >= 0 && idx < (int)row.size()) return row[idx];
        return "";
    }

    // 字面量
    if (auto lit = dynamic_cast<Parser::LiteralNode*>(node)) {
        return lit->value;
    }

    // 算术运算: +, -, *, /
    if (auto bin = dynamic_cast<Parser::BinaryOperatorNode*>(node)) {
        if (bin->op == "+" || bin->op == "-" || bin->op == "*" || bin->op == "/") {
            std::string lv = evalExpr(bin->left.get(), row);
            std::string rv = evalExpr(bin->right.get(), row);
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

bool ProjectOperator::init() {
    if (!child_->init()) return false;

    auto childSchema = child_->getOutputSchema();

    if (targetFields_.empty()) {
        // SELECT *
        outputSchema_ = childSchema;
        keepIndices_.resize(childSchema.size());
        for (size_t i = 0; i < childSchema.size(); ++i) keepIndices_[i] = (int)i;
        computedExprs_.assign(childSchema.size(), nullptr);
        return true;
    }

    for (size_t t = 0; t < targetFields_.size(); ++t) {
        const auto& target = targetFields_[t];
        int idx = findCol(target);

        if (idx >= 0) {
            // 简单列引用
            keepIndices_.push_back(idx);
            outputSchema_.push_back(childSchema[idx]);
            computedExprs_.push_back(nullptr);
        } else {
            // 可能在 targetASTs 中有对应的别名表达式，尝试匹配
            Parser::ASTNode* matchedExpr = nullptr;
            std::string aliasName;

            // 在 targetASTs 中查找 AS 别名
            for (auto* astNode : targetASTs_) {
                if (auto bin = dynamic_cast<Parser::BinaryOperatorNode*>(astNode)) {
                    if (bin->op == "AS" && bin->right) {
                        if (auto aliasCol = dynamic_cast<Parser::ColumnRefNode*>(bin->right.get())) {
                            if (aliasCol->columnName == target) {
                                matchedExpr = bin->left.get();
                                aliasName = target;
                                break;
                            }
                        }
                    }
                }
                // 聚合函数: 函数名 = 输出列名
                if (auto func = dynamic_cast<Parser::FunctionCallNode*>(astNode)) {
                    if (func->functionName == target) {
                        // 聚合列来自上层 AggregateOperator，如果这里找不到，
                        // 说明没有走 Aggregate 路径。只能报错。
                    }
                }
            }

            if (matchedExpr) {
                Meta::FieldBlock fb;
                std::memset(&fb, 0, sizeof(fb));
                std::strncpy(fb.name, aliasName.c_str(), sizeof(fb.name) - 1);
                fb.type = 4; // VARCHAR for computed values
                outputSchema_.push_back(fb);
                keepIndices_.push_back(-1); // 标记为计算列
                computedExprs_.push_back(matchedExpr);
            } else {
                std::cerr << "Error: 表中不存在列 '" << target << "'。" << std::endl;
                return false;
            }
        }
    }

    return true;
}

bool ProjectOperator::next(std::vector<std::string>& row) {
    std::vector<std::string> childRow;
    if (!child_->next(childRow)) return false;

    row.clear();
    for (size_t i = 0; i < keepIndices_.size(); ++i) {
        int idx = keepIndices_[i];
        if (idx >= 0 && idx < (int)childRow.size()) {
            row.push_back(childRow[idx]);
        } else if (idx == -1 && computedExprs_[i]) {
            // 计算列: 对表达式求值
            row.push_back(evalExpr(computedExprs_[i], childRow));
        } else {
            row.push_back("");
        }
    }
    return true;
}

std::vector<Meta::FieldBlock> ProjectOperator::getOutputSchema() const {
    return outputSchema_;
}

} // namespace Execution
