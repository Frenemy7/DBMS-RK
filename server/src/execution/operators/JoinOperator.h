#ifndef JOIN_OPERATOR_H
#define JOIN_OPERATOR_H

#include "../../../include/execution/IOperator.h"
#include "../../../include/meta/TableMeta.h"
#include "../../../include/parser/ASTNode.h"
#include <memory>
#include <vector>
#include <string>

namespace Execution {

class JoinOperator : public IOperator {
public:
    JoinOperator(std::unique_ptr<IOperator> left, std::unique_ptr<IOperator> right,
                 const std::string& joinType, Parser::ASTNode* onCondition,
                 std::vector<Meta::FieldBlock> leftSchema, std::vector<Meta::FieldBlock> rightSchema);
    ~JoinOperator() override = default;

    bool init() override;
    bool next(std::vector<std::string>& row) override;
    std::vector<Meta::FieldBlock> getOutputSchema() const override;

private:
    std::unique_ptr<IOperator> leftChild;
    std::unique_ptr<IOperator> rightChild;
    std::string joinType;
    Parser::ASTNode* onCondition; // borrowed, not owned

    std::vector<Meta::FieldBlock> leftSchema;
    std::vector<Meta::FieldBlock> rightSchema;
    std::vector<Meta::FieldBlock> outputSchema;

    // Materialized data
    std::vector<std::vector<std::string>> leftRows;
    std::vector<std::vector<std::string>> rightRows;
    std::vector<bool> rightMatched; // for RIGHT JOIN tracking
    std::vector<bool> leftMatched;  // for LEFT JOIN tracking

    size_t leftIdx = 0;
    size_t rightIdx = 0;
    bool emittingNullRow = false; // for LEFT/RIGHT JOIN unmatched rows

    int findColumnIndex(const std::vector<Meta::FieldBlock>& schema, const std::string& name) const;
    bool evaluateCondition(Parser::ASTNode* cond, const std::vector<std::string>& combinedRow);
};

} // namespace Execution

#endif