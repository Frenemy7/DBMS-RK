#ifndef PROJECT_OPERATOR_H
#define PROJECT_OPERATOR_H

#include "../../../include/execution/IOperator.h"
#include "../../../include/parser/ASTNode.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace Execution {

    class ProjectOperator : public IOperator {
    private:
        std::unique_ptr<IOperator> child_;
        std::vector<std::string> targetFields_;
        std::vector<Parser::ASTNode*> targetASTs_;  // 完整的 SELECT 项 AST（用于求值）

        std::vector<Meta::FieldBlock> outputSchema_;
        std::vector<int> keepIndices_;
        std::vector<Parser::ASTNode*> computedExprs_; // keepIndices_[i]==-1 时对应的表达式

        // 表达式求值（复刻 FilterOperator 的逻辑）
        std::string evalExpr(Parser::ASTNode* node, const std::vector<std::string>& row);
        int findCol(const std::string& name) const;

    public:
        ProjectOperator(std::unique_ptr<IOperator> child,
                        const std::vector<std::string>& targets,
                        const std::vector<Parser::ASTNode*>& astTargets = {});
        ~ProjectOperator() override = default;

        bool init() override;
        bool next(std::vector<std::string>& row) override;
        std::vector<Meta::FieldBlock> getOutputSchema() const override;
    };

}
#endif
