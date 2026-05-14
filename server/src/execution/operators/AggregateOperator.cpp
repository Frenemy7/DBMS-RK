#include "AggregateOperator.h"
#include "../../../include/parser/SelectASTNode.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
// 聚合函数
namespace Execution {

AggregateOperator::AggregateOperator(std::unique_ptr<IOperator> child,
                                     const std::vector<Parser::ASTNode*>& groupByNodes,
                                     const std::vector<Parser::ASTNode*>& targetNodes,
                                     const std::vector<Meta::FieldBlock>& inputSchema)
    : child(std::move(child)), groupByNodes(groupByNodes),
      targetNodes(targetNodes), inputSchema(inputSchema) {}

int AggregateOperator::findColIndex(const std::string& name) const {
    for (size_t i = 0; i < inputSchema.size(); ++i) {
        if (name == inputSchema[i].name) return static_cast<int>(i);
    }
    return -1;
}

bool AggregateOperator::parseAggregates() {
    aggFuncs.clear();
    groupByCols.clear();
    outputSchema.clear();

    // 解析group by
    for (auto* node : groupByNodes) {
        if (auto col = dynamic_cast<Parser::ColumnRefNode*>(node)) {
            int idx = findColIndex(col->columnName);
            if (idx < 0) {
                std::cerr << "Error: GROUP BY column '" << col->columnName << "' not found." << std::endl;
                return false;
            }
            groupByCols.push_back(idx);
            outputSchema.push_back(inputSchema[idx]);
        }
    }

    for (auto* node : targetNodes) {
        if (auto bin = dynamic_cast<Parser::BinaryOperatorNode*>(node)) {
            if (bin->op == "AS") {
                node = bin->left.get();
            }
        }

        if (auto func = dynamic_cast<Parser::FunctionCallNode*>(node)) {
            std::string fn = func->functionName;
            std::transform(fn.begin(), fn.end(), fn.begin(), ::toupper);

            AggFuncDef def;
            def.funcName = fn;
            def.isDistinct = func->isDistinct;
            def.outputCol = static_cast<int>(outputSchema.size());

            if (fn == "COUNT" && func->arguments.empty()) {
                def.targetCol = -1; // COUNT(*)
            } else if (!func->arguments.empty()) {
                if (auto col = dynamic_cast<Parser::ColumnRefNode*>(func->arguments[0].get())) {
                    def.targetCol = findColIndex(col->columnName);
                } else {
                    def.targetCol = -2; // expression, not supported yet
                }
            } else {
                def.targetCol = -2;
            }

            Meta::FieldBlock fb;
            std::memset(&fb, 0, sizeof(fb));
            std::strncpy(fb.name, fn.c_str(), sizeof(fb.name) - 1);
            fb.type = (fn == "COUNT" || fn == "SUM") ? 1 : (fn == "AVG" ? 3 : 4);
            outputSchema.push_back(fb);
            aggFuncs.push_back(def);

        } else if (auto col = dynamic_cast<Parser::ColumnRefNode*>(node)) {
            int idx = findColIndex(col->columnName);
            if (idx >= 0) {
                bool found = false;
                for (size_t k = 0; k < outputSchema.size(); ++k) {
                    if (std::string(outputSchema[k].name) == col->columnName) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    outputSchema.push_back(inputSchema[idx]);
                    groupByCols.push_back(idx);
                }
            }
        }
    }

    if (groupByCols.empty() && aggFuncs.empty()) {
        outputSchema = inputSchema;
    }

    return true;
}

bool AggregateOperator::init() {
    if (!child || !child->init()) return false;
    if (!parseAggregates()) return false;

    // If no aggregates and no GROUP BY, just pass through
    if (aggFuncs.empty() && groupByCols.empty()) {
        return true; // next() will just delegate to child
    }

    // Read all rows and group them
    std::vector<std::string> row;
    std::map<std::string, GroupRow> groupMap;
    std::vector<std::string> groupKeysOrder;

    while (child->next(row)) {
        // Build group key
        std::string groupKey;
        for (int idx : groupByCols) {
            if (idx >= 0 && idx < (int)row.size()) {
                groupKey += row[idx] + "|";
            }
        }

        auto it = groupMap.find(groupKey);
        if (it == groupMap.end()) {
            GroupRow gr;
            for (int idx : groupByCols) {
                if (idx >= 0 && idx < (int)row.size()) gr.groupValues.push_back(row[idx]);
            }
            gr.rows.push_back(row);
            groupMap[groupKey] = gr;
            groupKeysOrder.push_back(groupKey);
        } else {
            it->second.rows.push_back(row);
        }
    }

    // Convert map to ordered vector
    groups.clear();
    for (const auto& key : groupKeysOrder) {
        groups.push_back(groupMap[key]);
    }

    // 若无 GROUP BY 但有聚合函数，丢弃上面的分组结果，将所有行归为一组
    if (groupByCols.empty() && !aggFuncs.empty()) {
        groups.clear();
        GroupRow gr;
        child->init();
        while (child->next(row)) {
            gr.rows.push_back(row);
        }
        groups.push_back(gr);
    }

    groupIdx = 0;
    return true;
}

bool AggregateOperator::next(std::vector<std::string>& row) {
    // Pass-through mode
    if (aggFuncs.empty() && groupByCols.empty()) {
        return child->next(row);
    }

    if (groupIdx >= groups.size()) return false;

    const auto& gr = groups[groupIdx++];
    row.clear();

    // Output: first GROUP BY columns, then aggregates
    for (size_t i = 0; i < groupByCols.size(); ++i) {
        if (i < gr.groupValues.size()) row.push_back(gr.groupValues[i]);
        else row.push_back("");
    }

    for (const auto& agg : aggFuncs) {
        if (agg.funcName == "COUNT") {
            if (agg.targetCol == -1) {
                row.push_back(std::to_string(gr.rows.size()));
            } else if (agg.isDistinct) {
                std::set<std::string> distinctVals;
                for (const auto& r : gr.rows) {
                    if (agg.targetCol >= 0 && agg.targetCol < (int)r.size())
                        distinctVals.insert(r[agg.targetCol]);
                }
                row.push_back(std::to_string(distinctVals.size()));
            } else {
                int cnt = 0;
                for (const auto& r : gr.rows) {
                    if (agg.targetCol >= 0 && agg.targetCol < (int)r.size() && !r[agg.targetCol].empty())
                        cnt++;
                }
                row.push_back(std::to_string(cnt));
            }
        } else if (agg.funcName == "SUM") {
            double sum = 0;
            for (const auto& r : gr.rows) {
                if (agg.targetCol >= 0 && agg.targetCol < (int)r.size()) {
                    try { sum += std::stod(r[agg.targetCol]); } catch (...) {}
                }
            }
            row.push_back(std::to_string(sum));
        } else if (agg.funcName == "AVG") {
            double sum = 0;
            int cnt = 0;
            for (const auto& r : gr.rows) {
                if (agg.targetCol >= 0 && agg.targetCol < (int)r.size() && !r[agg.targetCol].empty()) {
                    try { sum += std::stod(r[agg.targetCol]); cnt++; } catch (...) {}
                }
            }
            double avg = cnt > 0 ? sum / cnt : 0;
            std::ostringstream oss;
            oss << avg;
            row.push_back(oss.str());
        } else if (agg.funcName == "MAX") {
            double maxVal = -1e300;
            bool hasVal = false;
            for (const auto& r : gr.rows) {
                if (agg.targetCol >= 0 && agg.targetCol < (int)r.size() && !r[agg.targetCol].empty()) {
                    try { double v = std::stod(r[agg.targetCol]); if (v > maxVal) maxVal = v; hasVal = true; } catch (...) {}
                }
            }
            row.push_back(hasVal ? std::to_string(maxVal) : "");
        } else if (agg.funcName == "MIN") {
            double minVal = 1e300;
            bool hasVal = false;
            for (const auto& r : gr.rows) {
                if (agg.targetCol >= 0 && agg.targetCol < (int)r.size() && !r[agg.targetCol].empty()) {
                    try { double v = std::stod(r[agg.targetCol]); if (v < minVal) minVal = v; hasVal = true; } catch (...) {}
                }
            }
            row.push_back(hasVal ? std::to_string(minVal) : "");
        } else {
            row.push_back("");
        }
    }

    return true;
}

std::vector<Meta::FieldBlock> AggregateOperator::getOutputSchema() const {
    return outputSchema;
}

} // namespace Execution
