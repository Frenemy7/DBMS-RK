#ifndef FILTER_OPERATOR_H
#define FILTER_OPERATOR_H

#include "../../../include/execution/IOperator.h"
#include "../../../include/parser/ASTNode.h" 
#include <memory>
#include <vector>
#include <string>

namespace Execution {

    class FilterOperator : public IOperator {
    private:
        std::unique_ptr<IOperator> child_;
        Parser::ASTNode* conditionTree_; // WHERE 后面的条件树
        std::vector<Meta::FieldBlock> childSchema_;

        // 核心递归评估引擎 (返回该行是否满足条件)
        bool evaluate(const std::vector<std::string>& row, Parser::ASTNode* node);
        
        // 辅助引擎：将叶子节点(字段名或字面量常量)榨取为真实的字符串值
        std::string evaluateValue(const std::vector<std::string>& row, Parser::ASTNode* node);
        
        // 工具：根据列名从当前行数组中提取对应的数据
        std::string getValueFromRow(const std::vector<std::string>& row, const std::string& colName);

    public:
        FilterOperator(std::unique_ptr<IOperator> child, Parser::ASTNode* condition);
        ~FilterOperator() override = default;

        bool init() override;
        bool next(std::vector<std::string>& row) override;
        std::vector<Meta::FieldBlock> getOutputSchema() const override;
    };

}
#endif // FILTER_OPERATOR_H