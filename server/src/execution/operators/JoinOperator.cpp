#include "JoinOperator.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>

namespace Execution {

JoinOperator::JoinOperator(std::unique_ptr<IOperator> left, std::unique_ptr<IOperator> right,
                           const std::string& joinType, Parser::ASTNode* onCondition,
                           std::vector<Meta::FieldBlock> leftSchema, std::vector<Meta::FieldBlock> rightSchema)
    : leftChild(std::move(left)), rightChild(std::move(right)),
      joinType(joinType), onCondition(onCondition),
      leftSchema(std::move(leftSchema)), rightSchema(std::move(rightSchema)) {}

bool JoinOperator::init() {
    if (!leftChild || !rightChild) return false;
    if (!leftChild->init() || !rightChild->init()) return false;

    // Build output schema
    outputSchema.clear();
    for (auto& f : leftSchema) outputSchema.push_back(f);
    for (auto& f : rightSchema) outputSchema.push_back(f);

    // Materialize all left rows
    leftRows.clear();
    std::vector<std::string> row;
    while (leftChild->next(row)) {
        leftRows.push_back(row);
    }

    // Materialize all right rows
    rightRows.clear();
    while (rightChild->next(row)) {
        rightRows.push_back(row);
    }

    rightMatched.assign(rightRows.size(), false);
    leftMatched.assign(leftRows.size(), false);

    leftIdx = 0;
    rightIdx = 0;
    emittingNullRow = false;

    return true;
}

int JoinOperator::findColumnIndex(const std::vector<Meta::FieldBlock>& schema, const std::string& name) const {
    for (size_t i = 0; i < schema.size(); ++i) {
        if (name == schema[i].name) return static_cast<int>(i);
    }
    return -1;
}

bool JoinOperator::evaluateCondition(Parser::ASTNode* cond, const std::vector<std::string>& combinedRow) {
    if (!cond) return true; // CROSS JOIN: no condition → always match

    // Binary operator
    if (auto bin = dynamic_cast<Parser::BinaryOperatorNode*>(cond)) {
        if (bin->op == "AND") {
            return evaluateCondition(bin->left.get(), combinedRow) &&
                   evaluateCondition(bin->right.get(), combinedRow);
        }
        if (bin->op == "OR") {
            return evaluateCondition(bin->left.get(), combinedRow) ||
                   evaluateCondition(bin->right.get(), combinedRow);
        }

        // 提取左右值（支持列引用、字面量、递归算术表达式）
        std::function<std::string(Parser::ASTNode*)> evalVal;
        evalVal = [this, &combinedRow, &evalVal](Parser::ASTNode* node) -> std::string {
            if (!node) return "";
            if (auto col = dynamic_cast<Parser::ColumnRefNode*>(node)) {
                int idx = findColumnIndex(outputSchema, col->columnName);
                if (idx >= 0 && idx < (int)combinedRow.size()) return combinedRow[idx];
                return "";
            }
            if (auto lit = dynamic_cast<Parser::LiteralNode*>(node)) {
                return lit->value;
            }
            // 递归处理算术运算
            if (auto b = dynamic_cast<Parser::BinaryOperatorNode*>(node)) {
                if (b->op == "+" || b->op == "-" || b->op == "*" || b->op == "/") {
                    std::string lv = evalVal(b->left.get());
                    std::string rv = evalVal(b->right.get());
                    try {
                        double ln = std::stod(lv), rn = std::stod(rv);
                        if (b->op == "+") return std::to_string(ln + rn);
                        if (b->op == "-") return std::to_string(ln - rn);
                        if (b->op == "*") return std::to_string(ln * rn);
                        if (b->op == "/") return rn != 0 ? std::to_string(ln / rn) : "0";
                    } catch (...) { return "0"; }
                }
            }
            return "";
        };

        std::string leftVal = evalVal(bin->left.get());
        std::string rightVal = evalVal(bin->right.get());

        // Normalize: strip quotes
        auto strip = [](const std::string& s) -> std::string {
            if (s.size() >= 2 && s.front() == '\'' && s.back() == '\'') return s.substr(1, s.size() - 2);
            return s;
        };
        leftVal = strip(leftVal);
        rightVal = strip(rightVal);

        // Try numeric comparison
        double dl = 0, dr = 0;
        bool numL = false, numR = false;
        try { dl = std::stod(leftVal); numL = true; } catch (...) {}
        try { dr = std::stod(rightVal); numR = true; } catch (...) {}

        if (bin->op == "=") {
            if (numL && numR) return std::fabs(dl - dr) < 1e-9;
            return leftVal == rightVal;
        }
        if (bin->op == "!=" || bin->op == "<>") {
            if (numL && numR) return std::fabs(dl - dr) >= 1e-9;
            return leftVal != rightVal;
        }
        if (numL && numR) {
            if (bin->op == ">") return dl > dr;
            if (bin->op == "<") return dl < dr;
            if (bin->op == ">=") return dl >= dr;
            if (bin->op == "<=") return dl <= dr;
        }

        return false;
    }

    return false;
}

bool JoinOperator::next(std::vector<std::string>& row) {
    // =====================================================
    //  LEFT JOIN
    // =====================================================
    if (joinType == "LEFT") {
        if (!emittingNullRow) {
            // 阶段一：输出所有匹配行
            while (leftIdx < leftRows.size()) {
                while (rightIdx < rightRows.size()) {
                    std::vector<std::string> combined;
                    combined.insert(combined.end(), leftRows[leftIdx].begin(), leftRows[leftIdx].end());
                    combined.insert(combined.end(), rightRows[rightIdx].begin(), rightRows[rightIdx].end());

                    bool matched = evaluateCondition(onCondition, combined);
                    rightIdx++;

                    if (matched) {
                        leftMatched[leftIdx] = true;
                        rightMatched[rightIdx - 1] = true;
                        row = std::move(combined);
                        return true;
                    }
                }
                // 右表扫完，前进到下一左行
                leftIdx++;
                rightIdx = 0;
            }
            // 阶段二：输出未匹配的左行（补 NULL）
            emittingNullRow = true;
        }
        // 阶段二持续：逐行输出未匹配的左行
        while (leftIdx < leftRows.size()) {
            if (!leftMatched[leftIdx]) {
                std::vector<std::string> combined;
                combined.insert(combined.end(), leftRows[leftIdx].begin(), leftRows[leftIdx].end());
                for (size_t j = 0; j < rightSchema.size(); ++j) combined.push_back("");
                leftMatched[leftIdx] = true;
                leftIdx++;
                row = std::move(combined);
                return true;
            }
            leftIdx++;
        }
        return false;
    }

    // =====================================================
    //  RIGHT JOIN
    // =====================================================
    if (joinType == "RIGHT") {
        if (!emittingNullRow) {
            while (leftIdx < leftRows.size()) {
                while (rightIdx < rightRows.size()) {
                    std::vector<std::string> combined;
                    combined.insert(combined.end(), leftRows[leftIdx].begin(), leftRows[leftIdx].end());
                    combined.insert(combined.end(), rightRows[rightIdx].begin(), rightRows[rightIdx].end());

                    bool matched = evaluateCondition(onCondition, combined);
                    rightIdx++;

                    if (matched) {
                        leftMatched[leftIdx] = true;
                        rightMatched[rightIdx - 1] = true;
                        row = std::move(combined);
                        return true;
                    }
                }
                leftIdx++;
                rightIdx = 0;
            }
            emittingNullRow = true;
        }
        while (rightIdx < rightRows.size()) {
            if (!rightMatched[rightIdx]) {
                std::vector<std::string> combined;
                for (size_t i = 0; i < leftSchema.size(); ++i) combined.push_back("");
                combined.insert(combined.end(), rightRows[rightIdx].begin(), rightRows[rightIdx].end());
                rightMatched[rightIdx] = true;
                rightIdx++;
                row = std::move(combined);
                return true;
            }
            rightIdx++;
        }
        return false;
    }

    // =====================================================
    //  INNER JOIN / CROSS JOIN
    // =====================================================
    while (leftIdx < leftRows.size()) {
        while (rightIdx < rightRows.size()) {
            std::vector<std::string> combined;
            combined.insert(combined.end(), leftRows[leftIdx].begin(), leftRows[leftIdx].end());
            combined.insert(combined.end(), rightRows[rightIdx].begin(), rightRows[rightIdx].end());

            rightIdx++;

            if (evaluateCondition(onCondition, combined)) {
                row = std::move(combined);
                return true;
            }
        }
        leftIdx++;
        rightIdx = 0;
    }

    return false;
}

std::vector<Meta::FieldBlock> JoinOperator::getOutputSchema() const {
    return outputSchema;
}

} // namespace Execution