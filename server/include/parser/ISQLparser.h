#ifndef ISQL_PARSER_H
#define ISQL_PARSER_H

#include "ASTNode.h"
#include <memory>
#include <string>

namespace Parser {

    class ISQLParser {
    public:
        virtual ~ISQLParser() = default;

        /**
         * @brief 将 SQL 文本解析为 AST
         * @param sql 用户输入的原生 SQL 字符串
         * @return 成功返回指向 AST 根节点的 unique_ptr，语法错误返回 nullptr
         */
        virtual std::unique_ptr<ASTNode> parse(const std::string& sql) = 0;
    };

} 
#endif // ISQL_PARSER_H