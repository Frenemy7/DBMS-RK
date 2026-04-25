#ifndef PROJECT_OPERATOR_H
#define PROJECT_OPERATOR_H

#include "../../../include/execution/IOperator.h"
#include "../../../include/parser/ASTNode.h"
#include <memory>
#include <vector>
#include <string>

namespace Execution {

    class ProjectOperator : public IOperator {
    private:
        std::unique_ptr<IOperator> child_; // 流水线的上一环（比如 Scan 或 Filter）
        std::vector<std::string> targetFields_; // 用户想要查询的列名 (如 "name", "age")
        
        std::vector<Meta::FieldBlock> outputSchema_; // 裁剪后的表头
        std::vector<int> keepIndices_; // 记录需要保留的列在原数据中的下标

    public:
        ProjectOperator(std::unique_ptr<IOperator> child, const std::vector<std::string>& targets);
        ~ProjectOperator() override = default;

        bool init() override;
        bool next(std::vector<std::string>& row) override;
        std::vector<Meta::FieldBlock> getOutputSchema() const override;
    };

}
#endif