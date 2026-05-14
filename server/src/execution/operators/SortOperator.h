#ifndef SORT_OPERATOR_H
#define SORT_OPERATOR_H

#include "../../../include/execution/IOperator.h"
#include "../../../include/meta/TableMeta.h"
#include "../../../include/parser/SelectASTNode.h"
#include <memory>
#include <vector>
#include <string>

namespace Execution {

class SortOperator : public IOperator {
public:
    SortOperator(std::unique_ptr<IOperator> child,
                 const std::vector<Parser::OrderByItem>& orderBy,
                 const std::vector<Meta::FieldBlock>& inputSchema);
    ~SortOperator() override = default;

    bool init() override;
    bool next(std::vector<std::string>& row) override;
    std::vector<Meta::FieldBlock> getOutputSchema() const override;

private:
    std::unique_ptr<IOperator> child;
    std::vector<Parser::OrderByItem> orderByItems;
    std::vector<Meta::FieldBlock> schema;
    std::vector<std::vector<std::string>> sortedRows;
    size_t rowIdx = 0;

    int findColIndex(const std::string& name) const;
};

} // namespace Execution

#endif
