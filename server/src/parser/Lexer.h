#ifndef LEXER_H
#define LEXER_H

#include "../../include/parser/Token.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace Parser {

    class Lexer {
    private:
        std::string sql; // 完整的原始 SQL 字符串
        int currentPos; // sql中当前扫描的char字符位置

        // 采用哈希表，全局关键字字典，将读取到的字符，绑定字典
        static const std::unordered_map<std::string, TokenType> keywordMap;

        // 处理之后的token结果
        std::vector<Token> tokens;

    private:
        // 将 SQL 彻底切分为 Token 数组 核心函数
        void tokenize();

        // 工具函数
        void skipSpace(); // 跳过空格和换行
        
        Token scanNumber(); // 提取数字
        Token scanString(); // 提取单引号包裹的字符串
        Token scanIdentifierOrKeyword(); // 提取连续的字母 (如 SELECT, student)
        Token scanSymbol(); // 提取符号

        

    public:
        // 构造函数：接收 SQL 字符串并初始化游标
        explicit Lexer(const std::string& sql);

        // 获取token
        const std::vector<Token>& getTokens();
    };
    

} // namespace Parser

#endif // LEXER_H