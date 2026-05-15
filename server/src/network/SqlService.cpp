#include "../../include/network/SqlService.h"

#include "../execution/ExecutorFactory.h"
#include "../execution/QueryBuilder.h"
#include "../execution/operators/FilterOperator.h"
#include "../execution/operators/ProjectOperator.h"
#include "../execution/operators/SortOperator.h"
#include "../execution/operators/AggregateOperator.h"
#include "../parser/SQLParserImpl.h"
#include "../../include/parser/SelectASTNode.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace Network {

    namespace {
        class StreamCapture {
        public:
            StreamCapture()
                : coutOld_(std::cout.rdbuf(out_.rdbuf())),
                  cerrOld_(std::cerr.rdbuf(err_.rdbuf())) {}

            ~StreamCapture() {
                std::cout.rdbuf(coutOld_);
                std::cerr.rdbuf(cerrOld_);
            }

            std::string output() const {
                std::string text = out_.str();
                const std::string error = err_.str();
                if (!error.empty()) {
                    if (!text.empty() && text.back() != '\n') text += '\n';
                    text += error;
                }
                return text;
            }

        private:
            std::ostringstream out_;
            std::ostringstream err_;
            std::streambuf* coutOld_;
            std::streambuf* cerrOld_;
        };

        std::string escapeCell(const std::string& value) {
            std::string result;
            for (char ch : value) {
                if (ch == '\\') result += "\\\\";
                else if (ch == '\t') result += "\\t";
                else if (ch == '\n') result += "\\n";
                else if (ch == '\r') result += "\\r";
                else result += ch;
            }
            return result;
        }
    }

    SqlService::SqlService(Catalog::ICatalogManager* catalog,
                           Storage::IStorageEngine* storage,
                           Integrity::IIntegrityManager* integrity,
                           Maintenance::IDatabaseMaintenance* maintenance,
                           Security::ISecurityManager* security,
                           Index::IIndexManager* index)
        : catalog_(catalog),
          storage_(storage),
          integrity_(integrity),
          maintenance_(maintenance),
          security_(security),
          index_(index) {}

    bool SqlService::login(const std::string& username, const std::string& password) {
        return security_ && security_->authenticate(username, password);
    }

    SqlResult SqlService::execute(const std::string& sql, const std::string& username) {
        std::lock_guard<std::mutex> lock(executionMutex_);

        SqlResult result;
        Parser::SQLParserImpl parser;

        try {
            auto astTree = parser.parse(sql);
            if (!astTree) {
                result.text = "Error: empty SQL.";
                result.currentDatabase = catalog_ ? catalog_->getCurrentDatabase() : "";
                result.text = encodeResponse(result);
                return result;
            }

            if (security_ && security_->getRole(username) != Security::Role::ADMIN && isAdminOnly(astTree->getType())) {
                result.text = "Error: permission denied. ADMIN role required.";
                result.currentDatabase = catalog_ ? catalog_->getCurrentDatabase() : "";
                result.text = encodeResponse(result);
                return result;
            }

            if (astTree->getType() == Parser::SQLType::SELECT) {
                result.table = buildSelectTable(astTree.get());
            }

            StreamCapture capture;
            auto executor = Execution::ExecutorFactory::createExecutor(
                std::move(astTree), catalog_, storage_, integrity_, maintenance_, security_, index_);

            if (!executor) {
                result.text = "Error: executor not found.";
                result.currentDatabase = catalog_ ? catalog_->getCurrentDatabase() : "";
                result.text = encodeResponse(result);
                return result;
            }

            result.success = executor->execute();
            result.text = capture.output();
            if (result.text.empty()) {
                result.text = result.success ? "Query OK." : "Query failed.";
            }
        } catch (const std::exception& e) {
            result.success = false;
            result.text = std::string("Error: ") + e.what();
        }

        if (catalog_) {
            result.currentDatabase = catalog_->getCurrentDatabase();
        }
        result.text = encodeResponse(result);
        return result;
    }

    bool SqlService::isAdminOnly(Parser::SQLType type) const {
        switch (type) {
            case Parser::SQLType::CREATE_TABLE:
            case Parser::SQLType::DROP_TABLE:
            case Parser::SQLType::ALTER_TABLE:
            case Parser::SQLType::CREATE_DATABASE:
            case Parser::SQLType::DROP_DATABASE:
            case Parser::SQLType::CREATE_USER:
            case Parser::SQLType::DROP_USER:
            case Parser::SQLType::GRANT_ROLE:
            case Parser::SQLType::REVOKE_ROLE:
            case Parser::SQLType::BACKUP_DATABASE:
            case Parser::SQLType::RESTORE_DATABASE:
                return true;
            default:
                return false;
        }
    }

    QueryTable SqlService::buildSelectTable(Parser::ASTNode* ast) {
        auto* selectAst = dynamic_cast<Parser::SelectASTNode*>(ast);
        if (!selectAst) return {};
        return runSelect(selectAst);
    }

    QueryTable SqlService::runSelect(Parser::SelectASTNode* selectAst) {
        QueryTable table;

        std::unique_ptr<Execution::IOperator> rootOp = Execution::buildTableSource(
            selectAst->fromSource.get(),
            catalog_,
            storage_,
            index_,
            selectAst->whereExpressionTree.get());

        if (!rootOp) return table;

        std::string mainAlias;
        std::vector<Meta::FieldBlock> mainSchema;
        if (auto tableNode = dynamic_cast<Parser::TableNode*>(selectAst->fromSource.get())) {
            mainAlias = tableNode->alias.empty() ? tableNode->tableName : tableNode->alias;
            if (catalog_->hasTable(tableNode->tableName)) {
                mainSchema = catalog_->getFields(tableNode->tableName);
                std::sort(mainSchema.begin(), mainSchema.end(),
                    [](const Meta::FieldBlock& a, const Meta::FieldBlock& b) { return a.order < b.order; });
            }
        }

        if (selectAst->whereExpressionTree) {
            auto filter = std::make_unique<Execution::FilterOperator>(
                std::move(rootOp), selectAst->whereExpressionTree.get());
            filter->setSubqueryContext(catalog_, storage_);
            filter->setOuterContext(nullptr, mainSchema, mainAlias);
            rootOp = std::move(filter);
        }

        if (!rootOp->init()) return table;
        auto inSchema = rootOp->getOutputSchema();

        const bool hasGroupBy = !selectAst->groupByFields.empty();
        bool hasAggregates = false;
        for (const auto& target : selectAst->targetFields) {
            if (dynamic_cast<Parser::FunctionCallNode*>(target.get())) {
                hasAggregates = true;
                break;
            }
            if (auto bin = dynamic_cast<Parser::BinaryOperatorNode*>(target.get())) {
                if (bin->op == "AS" && dynamic_cast<Parser::FunctionCallNode*>(bin->left.get())) {
                    hasAggregates = true;
                    break;
                }
            }
        }

        if (hasGroupBy || hasAggregates) {
            std::vector<Parser::ASTNode*> groupByPtrs;
            for (auto& groupBy : selectAst->groupByFields) groupByPtrs.push_back(groupBy.get());

            std::vector<Parser::ASTNode*> targetPtrs;
            for (auto& target : selectAst->targetFields) targetPtrs.push_back(target.get());

            rootOp = std::make_unique<Execution::AggregateOperator>(
                std::move(rootOp), groupByPtrs, targetPtrs, inSchema);

            if (selectAst->havingExpressionTree) {
                auto filter = std::make_unique<Execution::FilterOperator>(
                    std::move(rootOp), selectAst->havingExpressionTree.get());
                filter->setSubqueryContext(catalog_, storage_);
                rootOp = std::move(filter);
            }
        }

        if (!selectAst->orderByItems.empty()) {
            rootOp = std::make_unique<Execution::SortOperator>(
                std::move(rootOp), selectAst->orderByItems, inSchema);
        }

        std::vector<std::string> targetCols;
        std::vector<Parser::ASTNode*> targetASTs;
        bool hasStar = false;
        for (auto& target : selectAst->targetFields) {
            targetASTs.push_back(target.get());
            if (auto bin = dynamic_cast<Parser::BinaryOperatorNode*>(target.get())) {
                if (bin->op == "AS" && bin->right) {
                    if (auto aliasCol = dynamic_cast<Parser::ColumnRefNode*>(bin->right.get())) {
                        targetCols.push_back(aliasCol->columnName);
                        continue;
                    }
                }
            }
            if (auto col = dynamic_cast<Parser::ColumnRefNode*>(target.get())) {
                if (col->columnName == "*") {
                    hasStar = true;
                    break;
                }
                targetCols.push_back(col->columnName);
            } else if (auto func = dynamic_cast<Parser::FunctionCallNode*>(target.get())) {
                targetCols.push_back(func->functionName);
            }
        }
        if (hasStar) targetCols.clear();

        rootOp = std::make_unique<Execution::ProjectOperator>(
            std::move(rootOp), targetCols, targetASTs);

        if (!rootOp->init()) return table;
        const auto schema = rootOp->getOutputSchema();
        for (const auto& field : schema) {
            table.headers.push_back(field.name);
        }

        std::vector<std::string> row;
        while (rootOp->next(row)) {
            table.rows.push_back(row);
        }

        return table;
    }

    std::string SqlService::encodeResponse(const SqlResult& result) const {
        std::ostringstream response;
        response << "STATUS\t" << (result.success ? "OK" : "ERROR") << '\n';
        response << "DATABASE\t" << escapeCell(result.currentDatabase) << '\n';
        response << "MESSAGE_BEGIN\n" << result.text;
        if (!result.text.empty() && result.text.back() != '\n') response << '\n';
        response << "MESSAGE_END\n";

        if (!result.table.headers.empty()) {
            response << "RESULT_TABLE_BEGIN\n";
            response << "COLUMNS\t" << result.table.headers.size() << '\n';
            response << "HEADERS";
            for (const auto& header : result.table.headers) response << '\t' << escapeCell(header);
            response << '\n';
            response << "ROWS\t" << result.table.rows.size() << '\n';
            for (const auto& row : result.table.rows) {
                response << "ROW";
                for (const auto& cell : row) response << '\t' << escapeCell(cell);
                response << '\n';
            }
            response << "RESULT_TABLE_END\n";
        }

        return response.str();
    }

}
