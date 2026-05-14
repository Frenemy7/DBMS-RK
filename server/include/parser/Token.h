#ifndef PARSER_TOKEN_H
#define PARSER_TOKEN_H

#include <string>

namespace Parser {

    enum class TokenType {
        // --- 关键字 (Keywords) ---
        KW_SELECT, KW_FROM, KW_WHERE, KW_INSERT, KW_INTO, KW_VALUES,
        KW_CREATE, KW_TABLE, KW_DROP, KW_DELETE, KW_UPDATE, KW_SET,
        KW_INT, KW_CHAR, KW_VARCHAR, KW_DOUBLE, KW_BOOLEAN, KW_DATETIME,
        KW_ALTER, KW_ADD, KW_MODIFY, KW_COLUMN,
        KW_USER, KW_IDENTIFIED, KW_GRANT, KW_REVOKE, KW_BACKUP, KW_RESTORE, KW_TO,
        KW_PRIMARY, KW_KEY, KW_UNIQUE, KW_NOT, KW_NULL, KW_DEFAULT, KW_CHECK, KW_IDENTITY,
        KW_AND, KW_OR, KW_JOIN, KW_ON, KW_DISTINCT, KW_ORDER, KW_BY, KW_GROUP, KW_HAVING,
        KW_DATABASE, KW_USE,
        // JOIN 类型
        KW_LEFT, KW_RIGHT, KW_INNER, KW_FULL, KW_CROSS,
        // 别名
        KW_AS,
        // 排序方向
        KW_ASC, KW_DESC,
        // 子查询相关
        KW_IN, KW_EXISTS, KW_ANY, KW_ALL,
        // 表达式
        KW_LIKE, KW_IS, KW_BETWEEN,
        // 聚合函数
        KW_COUNT, KW_SUM, KW_AVG, KW_MAX, KW_MIN,

        // --- 标识符 (Identifiers) ---
        IDENTIFIER,

        // --- 字面量常量 (Literals) ---
        NUMBER_LITERAL,
        STRING_LITERAL,

        // --- 运算符与符号 (Symbols) ---
        SYM_COMMA,       // ,
        SYM_SEMICOLON,   // ;
        SYM_LPAREN,      // (
        SYM_RPAREN,      // )
        SYM_STAR,        // *
        SYM_DOT,         // .
        SYM_PLUS,        // +
        SYM_MINUS,       // -
        SYM_SLASH,       // /
        SYM_EQ,          // =
        SYM_GT,          // >
        SYM_LT,          // <
        SYM_GE,          // >=
        SYM_LE,          // <=
        SYM_NEQ,         // != 或 <>

        // --- 特殊标记 ---
        END_OF_FILE,
        INVALID
    };

    struct Token {
        TokenType type;
        std::string value;
        int column;
    };

} // namespace Parser

#endif // PARSER_TOKEN_H
