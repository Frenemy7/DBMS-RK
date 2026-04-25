#include "FilterOperator.h"
#include <iostream>

namespace Execution {

    FilterOperator::FilterOperator(std::unique_ptr<IOperator> child, Parser::ASTNode* condition)
        : child_(std::move(child)), conditionTree_(condition) {}

    bool FilterOperator::init() {
        if (!child_->init()) return false;
        childSchema_ = child_->getOutputSchema(); // 将下层算子的表头结构缓存到内存中
        return true;
    }

    bool FilterOperator::next(std::vector<std::string>& row) {
        // 通过 while 循环，持续调用下层算子的 next() 获取数据
        while (child_->next(row)) {
            // 对获取到的每一行数据，进行 WHERE 表达式树的真值计算
            if (evaluate(row, conditionTree_)) {
                return true; // 如果计算结果为 true，将这行数据返回给上层
            }
            // 如果为 false，row 的数据将被丢弃，while 循环进入下一次迭代，继续向硬盘索要下一行
        }
        return false; // 下层算子返回 false，表示文件已读到 EOF
    }

    std::vector<Meta::FieldBlock> FilterOperator::getOutputSchema() const {
        return childSchema_; // 过滤操作不改变列的物理结构，直接透传表头
    }

    // 从行数组中按列名提取数据
    std::string FilterOperator::getValueFromRow(const std::vector<std::string>& row, const std::string& colName) {
        for (size_t i = 0; i < childSchema_.size(); ++i) {
            // 对接你 ASTNode.h 中 ColumnDefinition 的 name 字段
            if (std::string(childSchema_[i].name) == colName) {
                if (i < row.size()) {
                    return row[i];
                }
            }
        }
        return ""; 
    }


    // 节点解析：从 AST 节点中提取真实的文本值
    std::string FilterOperator::evaluateValue(const std::vector<std::string>& row, Parser::ASTNode* node) {
        if (!node) return "";

        // 根据 ASTNode.h：如果是字段引用节点
        if (auto colRefNode = dynamic_cast<Parser::ColumnRefNode*>(node)) {
            return getValueFromRow(row, colRefNode->columnName);
        }

        // 根据 ASTNode.h：如果是字面量常量节点
        if (auto litNode = dynamic_cast<Parser::LiteralNode*>(node)) {
            // 根据 Lexer::scanString 的物理实现，单引号已被切除，此处直接返回底层文本
            return litNode->value;
        }

        return "";
    }


    // 递归执行引擎：计算表达式的布尔值
    bool FilterOperator::evaluate(const std::vector<std::string>& row, Parser::ASTNode* node) {
        if (!node) return true; // 对应没有 WHERE 子句的情况，默认全表放行

        // 根据 ASTNode.h 解析二元操作符节点
        if (auto binaryNode = dynamic_cast<Parser::BinaryOperatorNode*>(node)) {
            std::string op = binaryNode->op;

            // 分别递归获取左子树和右子树代表的真实字符串数据
            std::string leftVal = evaluateValue(row, binaryNode->left.get());
            std::string rightVal = evaluateValue(row, binaryNode->right.get());

            // 数值型比较逻辑：尝试通过 std::stod 将字符串转换为浮点数进行物理比较
            // 这是为了防止出现字符串比较导致的逻辑错误（如 "12" < "5" 成立的 Bug）
            try {
                if (!leftVal.empty() && !rightVal.empty()) {
                    double leftNum = std::stod(leftVal);
                    double rightNum = std::stod(rightVal);

                    // 这里的操作符严格对齐你 Token.h 中 SYM 系列的值
                    if (op == "=")  return leftNum == rightNum;
                    if (op == ">")  return leftNum > rightNum;
                    if (op == "<")  return leftNum < rightNum;
                    if (op == ">=") return leftNum >= rightNum;
                    if (op == "<=") return leftNum <= rightNum;
                    if (op == "!=" || op == "<>") return leftNum != rightNum;
                }
            } catch (...) {
                // 如果 std::stod 抛出异常，说明参与比较的数据包含非数字字符（如比较用户名）
                // 此时退化为 C++ 原生的字符串字典序比对
            }

            // 字符串比对逻辑
            if (op == "=")  return leftVal == rightVal;
            if (op == ">")  return leftVal > rightVal;
            if (op == "<")  return leftVal < rightVal;
            if (op == ">=") return leftVal >= rightVal;
            if (op == "<=") return leftVal <= rightVal;
            if (op == "!=" || op == "<>") return leftVal != rightVal;
        }

        // 如果节点类型超出当前解析范围，返回 false 阻止脏数据流出
        return false; 
    }

} // namespace Execution