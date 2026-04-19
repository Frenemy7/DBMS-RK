#ifndef ISQL_PARSER_H
#define ISQL_PARSER_H

#include "ASTNode.h"
#include <string>

namespace Parser {

    class ISQLParser {
    public:
        virtual ~ISQLParser() = default;

        // 核心接口：接收纯文本 SQL，返回结构化的 AST 节点指针。
        // 如果语法错误（比如少敲了括号），直接返回 nullptr。
        virtual ASTNode* parse(const std::string& sql) = 0;
    };

} // namespace Parser


#endif // ISQL_PARSER_H