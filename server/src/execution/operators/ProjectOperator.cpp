#include "ProjectOperator.h"
#include <iostream>

namespace Execution {

    ProjectOperator::ProjectOperator(std::unique_ptr<IOperator> child, const std::vector<std::string>& targets)
        : child_(std::move(child)), targetFields_(targets) {}

    bool ProjectOperator::init() {
        if (!child_->init()) return false;
        
        std::vector<Meta::FieldBlock> childSchema = child_->getOutputSchema();

        // 如果 targetFields_ 为空，代表是 SELECT *，保留所有列
        if (targetFields_.empty()) {
            outputSchema_ = childSchema;
            for (size_t i = 0; i < childSchema.size(); ++i) {
                keepIndices_.push_back(i);
            }
            return true;
        }

        // 如果是 SELECT name, age，计算映射下标
        for (const auto& target : targetFields_) {
            bool found = false;
            for (size_t i = 0; i < childSchema.size(); ++i) {
                if (std::string(childSchema[i].name) == target) {
                    keepIndices_.push_back(i);
                    outputSchema_.push_back(childSchema[i]);
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::cerr << "Error: 表中不存在列 '" << target << "'。" << std::endl;
                return false;
            }
        }
        return true;
    }

    bool ProjectOperator::next(std::vector<std::string>& row) {
        std::vector<std::string> childRow;
        
        // 向上层吐数据前，先把下层的数据拦截下来
        if (child_->next(childRow)) {
            row.clear();
            // 按照算好的下标，精准挑出需要的列拼成新行
            for (int index : keepIndices_) {
                if (index < childRow.size()) {
                    row.push_back(childRow[index]);
                } else {
                    row.push_back(""); // 防御性处理
                }
            }
            return true;
        }
        return false;
    }

    std::vector<Meta::FieldBlock> ProjectOperator::getOutputSchema() const {
        return outputSchema_;
    }

} // namespace Execution