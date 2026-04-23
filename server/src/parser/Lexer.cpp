#include "Lexer.h"
#include <algorithm>

namespace Parser {

    Lexer::Lexer(const std::string& sql) {
        this->sql = sql;
        this->currentPos = 0;
        tokenize(); 
    }

    Lexer::~Lexer() {}

    // 初始化静态关键字映射表
    const std::unordered_map<std::string, TokenType> Lexer::keywordMap = {
        {"SELECT", TokenType::KW_SELECT}, {"FROM", TokenType::KW_FROM},
        {"WHERE", TokenType::KW_WHERE}, {"CREATE", TokenType::KW_CREATE},
        {"TABLE", TokenType::KW_TABLE}, {"INSERT", TokenType::KW_INSERT},
        {"INTO", TokenType::KW_INTO}, {"VALUES", TokenType::KW_VALUES},
        {"UPDATE", TokenType::KW_UPDATE}, {"SET", TokenType::KW_SET},
        {"DELETE", TokenType::KW_DELETE}, {"DROP", TokenType::KW_DROP},
        {"JOIN", TokenType::KW_JOIN}, {"ON", TokenType::KW_ON},
        {"INT", TokenType::KW_INT}, {"CHAR", TokenType::KW_CHAR},
        {"VARCHAR", TokenType::KW_VARCHAR}, {"PRIMARY", TokenType::KW_PRIMARY},
        {"KEY", TokenType::KW_KEY}, {"NOT", TokenType::KW_NOT},
        {"NULL", TokenType::KW_NULL}, {"UNIQUE", TokenType::KW_UNIQUE},
        {"AND", TokenType::KW_AND}, {"OR", TokenType::KW_OR},
        {"DISTINCT", TokenType::KW_DISTINCT}, {"ORDER", TokenType::KW_ORDER},
        {"BY", TokenType::KW_BY}, {"GROUP", TokenType::KW_GROUP},
        {"HAVING", TokenType::KW_HAVING}
    };

    const std::vector<Token>& Lexer::getTokens() {
        return tokens;
    }

    void Lexer::tokenize() {
        tokens.clear();

        

        while (currentPos < sql.length()) {
            // 
            skipSpace();

            if (currentPos >= sql.length()) break;

            // 取出 词元 首字符
            char c = sql[currentPos];

            // 根据 c 的类型，调用不同的读词函数
            if (isalpha(c) || c == '_') {
                tokens.push_back(scanIdentifierOrKeyword());
            } 
            else if (isdigit(c)) {
                tokens.push_back(scanNumber());
            } 
            else if (c == '\'') {
                tokens.push_back(scanString());
            } 
            else {
                tokens.push_back(scanSymbol());
            }
        }

        // 压入结束标记，防止 Parser 越界
        tokens.push_back({TokenType::END_OF_FILE, "EOF", currentPos});
    }

    // 跳过空格、换行、制表符号
    void Lexer::skipSpace() {
        while (currentPos < sql.length() && isspace(sql[currentPos])) {
            // 控制索引，向后移
            currentPos++;
        }
    }

    // 读取单词
    Token Lexer::scanIdentifierOrKeyword() {
        int start = currentPos;// 开始读取位置
        std::string word = "";

        // 持续拼接单词（下划线也拼入单词）
        while (currentPos < sql.length() && (isalnum(sql[currentPos]) || sql[currentPos] == '_')) {
            word += sql[currentPos++];
        }

        // 大写转换
        std::string upperStr = word;
        std::transform(upperStr.begin(), upperStr.end(), upperStr.begin(), ::toupper);

        // 在映射里匹配关键词
        auto it = keywordMap.find(upperStr);
        if (it != keywordMap.end()) {
            return {it->second, upperStr, start}; // 匹配成功：系统关键字
        }
        return {TokenType::IDENTIFIER, word, start}; // 匹配失败：普通标识符（表名/列名）
    }

    // 读取数字
    Token Lexer::scanNumber() {
        int start = currentPos;
        std::string word = "";

        // 读取数字字节
        while (currentPos < sql.length() && isdigit(sql[currentPos])) {
            word += sql[currentPos++];
        }
        return {TokenType::NUMBER_LITERAL, word, start};
    }

    // 读取字符串，单引号里面的
    Token Lexer::scanString() {
        int start = currentPos;
        currentPos++; // 跳过起始的单引号
        std::string buffer = "";

        // 直到遇到闭合单引号前，所有内容均视为纯文本
        while (currentPos < sql.length() && sql[currentPos] != '\'') {
            buffer += sql[currentPos++];
        }

        if (currentPos >= sql.length()) {
            // 语法错误：字符串未闭合
            return {TokenType::INVALID, buffer, start};
        }
        
        currentPos++; // 跳过结束的单引号
        return {TokenType::STRING_LITERAL, buffer, start};
    }

    // 读取运算符
    Token Lexer::scanSymbol() {
        int start = currentPos;
        char c = sql[currentPos++];
        std::string buffer(1, c);

        // 检查是否为双字符符号（如 >=, <=, !=）
        if (currentPos < sql.length()) {
            char next = sql[currentPos];
            bool isDouble = false;
            if (c == '>' && next == '=') isDouble = true;
            else if (c == '<' && next == '=') isDouble = true;
            else if (c == '!' && next == '=') isDouble = true;
            else if (c == '<' && next == '>') isDouble = true;

            if (isDouble) {
                buffer += sql[currentPos++];
                if (buffer == ">=") return {TokenType::SYM_GE, ">=", start};
                if (buffer == "<=") return {TokenType::SYM_LE, "<=", start};
                if (buffer == "!=" || buffer == "<>") return {TokenType::SYM_NEQ, "!=", start};
            }
        }

        // 单字符符号判定
        switch (c) {
            case ',': return {TokenType::SYM_COMMA, ",", start};
            case ';': return {TokenType::SYM_SEMICOLON, ";", start};
            case '(': return {TokenType::SYM_LPAREN, "(", start};
            case ')': return {TokenType::SYM_RPAREN, ")", start};
            case '*': return {TokenType::SYM_STAR, "*", start};
            case '=': return {TokenType::SYM_EQ, "=", start};
            case '>': return {TokenType::SYM_GT, ">", start};
            case '<': return {TokenType::SYM_LT, "<", start};
            default:  return {TokenType::INVALID, buffer, start};
        }
    }
}
