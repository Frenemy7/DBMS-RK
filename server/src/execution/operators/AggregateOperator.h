#ifndef AGGREGATE_OPERATOR_H
#define AGGREGATE_OPERATOR_H

#include "../../../include/execution/IOperator.h"
#include "../../../include/meta/TableMeta.h"
#include "../../../include/parser/ASTNode.h"
#include <memory>
#include <vector>
#include <string>

namespace Execution {

struct AggFuncDef {
    std::string funcName;   // COUNT, SUM, AVG, MAX, MIN
    int targetCol;          // index into input schema (-1 for COUNT(*))
    int outputCol;          // index into output schema
    bool isDistinct;
};

class AggregateOperator : public IOperator {
public:
    AggregateOperator(std::unique_ptr<IOperator> child,
                      const std::vector<Parser::ASTNode*>& groupByNodes,
                      const std::vector<Parser::ASTNode*>& targetNodes,
                      const std::vector<Meta::FieldBlock>& inputSchema);
    ~AggregateOperator() override = default;

    bool init() override;
    bool next(std::vector<std::string>& row) override;
    std::vector<Meta::FieldBlock> getOutputSchema() const override;

private:
    std::unique_ptr<IOperator> child;
    std::vector<Parser::ASTNode*> groupByNodes;
    std::vector<Parser::ASTNode*> targetNodes;
    std::vector<Meta::FieldBlock> inputSchema;
    std::vector<Meta::FieldBlock> outputSchema;
    std::vector<AggFuncDef> aggFuncs;
    std::vector<int> groupByCols;

    struct GroupRow {
        std::vector<std::string> groupValues;
        std::vector<std::vector<std::string>> rows; // all rows in this group
    };
    std::vector<GroupRow> groups;
    size_t groupIdx = 0;

    int findColIndex(const std::string& name) const;
    bool parseAggregates();
};

} // namespace Execution

#endif