#ifndef SQL_PARSER_IMPL_H
#define SQL_PARSER_IMPL_H

#include "../../include/parser/ISQLParser.h"
#include "../../include/parser/SelectASTNode.h"
#include "Lexer.h"
#include <vector>
#include <memory>

namespace Parser {

    class SQLParserImpl : public ISQLParser {
    private:
        std::vector<Token> tokens;
        int currentTokenIndex;

        bool isAtEnd() const;
        const Token& peek() const;
        const Token& previous() const;
        const Token& consume();
        const Token& match(TokenType expected);
        bool check(TokenType type) const;

        // DDL
        std::unique_ptr<ASTNode> parseCreateDatabaseStatement();
        std::unique_ptr<ASTNode> parseDropDatabaseStatement();
        std::unique_ptr<ASTNode> parseBackupDatabaseStatement();
        std::unique_ptr<ASTNode> parseRestoreDatabaseStatement();
        std::unique_ptr<ASTNode> parseCreateUserStatement();
        std::unique_ptr<ASTNode> parseDropUserStatement();
        std::unique_ptr<ASTNode> parseGrantRevokeStatement();
        std::unique_ptr<ASTNode> parseUseDatabaseStatement();
        std::unique_ptr<ASTNode> parseCreateTableStatement();
        std::unique_ptr<ASTNode> parseDropTableStatement();
        std::unique_ptr<ASTNode> parseAlterTableStatement();

        // DML
        std::unique_ptr<ASTNode> parseInsertStatement();
        std::unique_ptr<ASTNode> parseUpdateStatement();
        std::unique_ptr<ASTNode> parseSelectStatement();
        std::unique_ptr<ASTNode> parseDeleteStatement();

        // SELECT helpers
        std::unique_ptr<ASTNode> parseSelectItem();
        std::unique_ptr<ASTNode> parseTableSource();
        std::unique_ptr<ASTNode> parseTableRef();
        OrderByItem parseOrderByItem();

        // Expression parsers (precedence climbing)
        std::unique_ptr<ASTNode> parseExpression();
        std::unique_ptr<ASTNode> parseOr();
        std::unique_ptr<ASTNode> parseAnd();
        std::unique_ptr<ASTNode> parseNot();
        std::unique_ptr<ASTNode> parseAddSub();
        std::unique_ptr<ASTNode> parseMulDiv();
        std::unique_ptr<ASTNode> parseComparison();
        std::unique_ptr<ASTNode> parsePrimary();
        std::unique_ptr<ASTNode> parseFunctionCall();

    public:
        SQLParserImpl();
        ~SQLParserImpl() override = default;
        std::unique_ptr<ASTNode> parse(const std::string& sql) override;
    };

} // namespace Parser

#endif // SQL_PARSER_IMPL_H
