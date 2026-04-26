#ifndef PARSER_TOKEN_H
#define PARSER_TOKEN_H

#include <string>

namespace Parser {

    // 使用枚举原因如下：
    // 1.大幅提高性能。若vector存储的是string字符串，在后续语句匹配上会造成极大的开销。枚举在C++中本质为int，int之间的比较开销大幅小于字符串
    // 2.区分 词元 属性
    // 3.通过token结构体，可以快速定位 token错误位置

    // 1. 穷举你的系统需要识别的所有合法词汇分类
    enum class TokenType {
        // --- 关键字 (Keywords) ---
        KW_SELECT, KW_FROM, KW_WHERE, KW_INSERT, KW_INTO, KW_VALUES,
        KW_CREATE, KW_TABLE, KW_DROP, KW_DELETE, KW_UPDATE, KW_SET,
        KW_INT, KW_CHAR, KW_VARCHAR, KW_DOUBLE, KW_BOOLEAN, KW_DATETIME,
        KW_PRIMARY, KW_KEY, KW_UNIQUE, KW_NOT, KW_NULL,
        KW_AND, KW_OR, KW_JOIN, KW_ON, KW_DISTINCT, KW_ORDER, KW_BY, KW_GROUP, KW_HAVING,
        KW_DATABASE, KW_USE,
        
        // --- 标识符 (Identifiers) ---
        IDENTIFIER,    // 表名、列名等自己取的名字

        // --- 字面量常量 (Literals) ---
        NUMBER_LITERAL, // 整数或浮点数，如 20, 0.8
        STRING_LITERAL, // 字符串，如 'Tom'

        // --- 运算符与符号 (Symbols) ---
        SYM_COMMA,       // ,
        SYM_SEMICOLON,   // ;
        SYM_LPAREN,      // (
        SYM_RPAREN,      // )
        SYM_STAR,        // *
        SYM_EQ,          // =
        SYM_GT,          // >
        SYM_LT,          // <
        SYM_GE,          // >=
        SYM_LE,          // <=
        SYM_NEQ,         // != 或 <>

        // --- 特殊标记 ---
        END_OF_FILE,     // 整个 SQL 解析完毕的终止符
        INVALID          // 词法错误（遇到了不认识的乱码字符）
    };

    // 2. Token 数据载体结构体
    struct Token {
        TokenType type;
        std::string value; // 实际抠出来的字符串内容
        
        // 可选：用于精确定位报错信息的坐标
        //int line;          // 行号（对于单句 SQL 通常为 1）
        int column;        // 该词在 SQL 语句中的起始字符位置
    };

} // namespace Parser

#endif // PARSER_TOKEN_H