#ifndef FILTER_OPERATOR_H
#define FILTER_OPERATOR_H

#include "../../../include/execution/IOperator.h"
#include "../../../include/parser/ASTNode.h"
#include <memory>
#include <vector>
#include <string>

namespace Catalog { class ICatalogManager; }
namespace Storage { class IStorageEngine; }

namespace Execution {

    class FilterOperator : public IOperator {
    private:
        std::unique_ptr<IOperator> child_;
        Parser::ASTNode* conditionTree_;
        std::vector<Meta::FieldBlock> childSchema_;

        Catalog::ICatalogManager* catalog_ = nullptr;
        Storage::IStorageEngine* storage_ = nullptr;

        // 关联子查询：外部查询的行数据 + 表结构
        const std::vector<std::string>* outerRow_ = nullptr;  // 不在 next() 中覆盖
        std::vector<Meta::FieldBlock> outerSchema_;
        std::string outerAlias_;

        bool evaluate(const std::vector<std::string>& row, Parser::ASTNode* node);
        std::string evaluateValue(const std::vector<std::string>& row, Parser::ASTNode* node);
        std::string getValueFromRow(const std::vector<std::string>& row, const std::string& colName);
        std::string getValueFromOuter(const std::string& colName) const;

        std::vector<std::vector<std::string>> executeSubquery(Parser::ASTNode* subqueryRoot,
                                                                const std::vector<std::string>& outerRow);

    public:
        FilterOperator(std::unique_ptr<IOperator> child, Parser::ASTNode* condition);
        ~FilterOperator() override = default;

        void setSubqueryContext(Catalog::ICatalogManager* catalog, Storage::IStorageEngine* storage);
        void setOuterContext(const std::vector<std::string>* outerRow,
                             const std::vector<Meta::FieldBlock>& outerSchema,
                             const std::string& outerAlias);

        bool init() override;
        bool next(std::vector<std::string>& row) override;
        std::vector<Meta::FieldBlock> getOutputSchema() const override;
    };

}
#endif
