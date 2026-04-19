#ifndef SQL_PARSER_IMPL_H
#define SQL_PARSER_IMPL_H

#include "../../include/parser/ASTNode.h"
#include "../../include/parser/ISQLParser.h"
#include <string>

namespace Parser {

    class SQLParserImpl : public ISQLParser {
    public:
        SQLParserImpl();
        ~SQLParserImpl() override;

        // 核心解析接口的实现
        ASTNode* parse(const std::string& sql) override;

    private:
        // 架构建议：组员可以在这里添加私有的辅助函数，让代码更整洁
        // 比如字符串去空格、转大写等工具函数
        std::string trim(const std::string& str);
        std::string toUpperCase(const std::string& str);
    };

} 
#endif // SQL_PARSER_IMPL_H