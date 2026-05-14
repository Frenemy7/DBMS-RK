#include "SortOperator.h"
#include <algorithm>
#include <iostream>

namespace Execution {

SortOperator::SortOperator(std::unique_ptr<IOperator> child,
                           const std::vector<Parser::OrderByItem>& orderBy,
                           const std::vector<Meta::FieldBlock>& inputSchema)
    : child(std::move(child)), orderByItems(orderBy), schema(inputSchema) {}

int SortOperator::findColIndex(const std::string& name) const {
    for (size_t i = 0; i < schema.size(); ++i) {
        if (name == schema[i].name) return static_cast<int>(i);
    }
    return -1;
}

bool SortOperator::init() {
    if (!child || !child->init()) return false;

    // Materialize all rows
    std::vector<std::string> row;
    while (child->next(row)) {
        sortedRows.push_back(row);
    }

    // Sort
    std::sort(sortedRows.begin(), sortedRows.end(),
              [this](const std::vector<std::string>& a, const std::vector<std::string>& b) {
        for (const auto& item : orderByItems) {
            int idx = findColIndex(item.field);
            if (idx < 0) continue;

            std::string va = (idx < (int)a.size()) ? a[idx] : "";
            std::string vb = (idx < (int)b.size()) ? b[idx] : "";

            // Try numeric comparison
            try {
                double da = std::stod(va);
                double db = std::stod(vb);
                if (da < db) return item.isAsc;
                if (da > db) return !item.isAsc;
            } catch (...) {
                // String comparison
                if (va < vb) return item.isAsc;
                if (va > vb) return !item.isAsc;
            }
        }
        return false; // equal
    });

    rowIdx = 0;
    return true;
}

bool SortOperator::next(std::vector<std::string>& row) {
    if (rowIdx >= sortedRows.size()) return false;
    row = sortedRows[rowIdx++];
    return true;
}

std::vector<Meta::FieldBlock> SortOperator::getOutputSchema() const {
    return schema;
}

} // namespace Execution
