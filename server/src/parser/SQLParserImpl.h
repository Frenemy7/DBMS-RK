#ifndef SQL_PARSER_IMPL_H
#define SQL_PARSER_IMPL_H

#include "../../include/parser/ISQLParser.h"
#include "Lexer.h"
#include <vector>
#include <memory>

namespace Parser {

    class SQLParserImpl : public ISQLParser {
    private:
        std::vector<Token> tokens; // 由 Lexer 生成的词元数组
        int currentTokenIndex; // 核心游标：当前正在处理的 Token 下标

        // === 底层游标控制机制 ===
        bool isAtEnd() const; // 探查是否到达 Token 数组末尾 (EOF)
        const Token& peek() const; // 探查当前游标指向的 Token（不移动游标）
        const Token& previous() const; // 获取游标刚刚跨过的上一个 Token
        const Token& consume(); // 吞噬当前 Token，并将游标后移一步
        
        // 核心校验函数：强制断言当前 Token 必须是 expected 类型，否则直接抛出语法异常
        const Token& match(TokenType expected); 
        bool check(TokenType type) const; // 仅检查当前 Token 类型，不抛异常也不移动游标

        // === 具体的语法分支解析函数 ===
        // 将不同类型 SQL 的解析逻辑拆分，严格遵循单一职责原则
        std::unique_ptr<ASTNode> parseCreateDatabaseStatement(); // 3.2.1
        std::unique_ptr<ASTNode> parseDropDatabaseStatement();   // 3.2.2
        std::unique_ptr<ASTNode> parseUseDatabaseStatement();    // 3.12.3 切换库
        
        std::unique_ptr<ASTNode> parseCreateTableStatement();    // 3.3.1
        std::unique_ptr<ASTNode> parseDropTableStatement();     // 3.3.3
        
        std::unique_ptr<ASTNode> parseInsertStatement();        // 3.5.1
        std::unique_ptr<ASTNode> parseUpdateStatement();        // 3.5.2
        std::unique_ptr<ASTNode> parseSelectStatement();        // 3.5.3
        std::unique_ptr<ASTNode> parseDeleteStatement();        // 3.5.4

        // === 核心：表达式与条件解析 (用于 WHERE 子句) ===
        // 采用递归下降法处理运算符优先级
        std::unique_ptr<ASTNode> parseExpression();
        std::unique_ptr<ASTNode> parseEquality();   // 处理 = , !=
        std::unique_ptr<ASTNode> parseComparison(); // 处理 > , < , >= , <=
        std::unique_ptr<ASTNode> parsePrimary();    // 处理最小单元：列名、数字、字符串


    public:
        SQLParserImpl();
        ~SQLParserImpl() override = default;

        // 顶层入口：接收纯文本 SQL，返回所有权独占的 AST 根节点智能指针
        std::unique_ptr<ASTNode> parse(const std::string& sql) override;
    };

} // namespace Parser

#endif // SQL_PARSER_IMPL_H

