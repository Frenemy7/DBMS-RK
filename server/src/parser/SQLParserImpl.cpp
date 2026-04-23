#include "SQLParserImpl.h"
#include "../../include/parser/DropTableASTNode.h"
#include "../../include/parser/CreateTableASTNode.h"
#include "../../include/parser/InsertASTNode.h"
#include "../../include/parser/SelectASTNode.h"
#include <stdexcept>

namespace Parser {

    // 构造函数：初始化游标位置
    SQLParserImpl::SQLParserImpl() : currentTokenIndex(0) {}

    std::unique_ptr<ASTNode> SQLParserImpl::parse(const std::string& sql) {
        // 实例化 Lexer 并进行物理词法切分
        Lexer lexer(sql);
        
        // 将切分好的词元流拷贝至 Parser 的核心状态中，游标归零
        tokens = lexer.getTokens();
        currentTokenIndex = 0;

        // 防御性拦截：空语句
        if (isAtEnd()) return nullptr;

        // 探查首个 Token，进行顶层语法分支路由
        const Token& firstToken = peek();
        
        switch (firstToken.type) {
            case TokenType::KW_DROP: // DROP
                return parseDropTableStatement();
            case TokenType::KW_CREATE: // CREATE
                return parseCreateTableStatement();
            // 后续在此处扩展 INSERT, SELECT, UPDATE, DELETE 的路由分支
            default:
                throw std::runtime_error("Syntax Error: Unsupported SQL statement starting with '" + 
                                         firstToken.value + "' at position " + std::to_string(firstToken.column));
        }
    }

    bool SQLParserImpl::isAtEnd() const {
        return peek().type == TokenType::END_OF_FILE;
    }

    const Token& SQLParserImpl::peek() const {
        return tokens[currentTokenIndex];
    }

    const Token& SQLParserImpl::previous() const {
        return tokens[currentTokenIndex - 1];
    }

    const Token& SQLParserImpl::consume() {
        if (!isAtEnd()) {
            currentTokenIndex++;
        }
        return previous(); // 返回被跨过的那一个 Token
    }

    bool SQLParserImpl::check(TokenType type) const {
        if (isAtEnd()) return false;
        return peek().type == type;
    }

    const Token& SQLParserImpl::match(TokenType expected) {
        // 如果当前指针指向的类型与预期一致，吞噬，前进
        if (check(expected)) {
            return consume();
        }
        
        // 类型不匹配，立即抛出包含物理坐标的语法错误
        throw std::runtime_error("Syntax Error: Expected different token near '" + 
                                 peek().value + "' at position " + std::to_string(peek().column));
    }

    // DROP 语句解析
    std::unique_ptr<ASTNode> SQLParserImpl::parseDropTableStatement() {
        // 线性严格断言匹配
        match(TokenType::KW_DROP);
        match(TokenType::KW_TABLE);
        
        // 提取表名 Token，保存其 value 数据
        const Token& tableNameToken = match(TokenType::IDENTIFIER);
        
        match(TokenType::SYM_SEMICOLON); // 语句必须以分号结束

        // 堆内存分配并挂载数据
        auto node = std::make_unique<DropTableASTNode>();
        node->tableName = tableNameToken.value;
        
        return node;
    }

    // CREATE 语句解析
    std::unique_ptr<ASTNode> SQLParserImpl::parseCreateTableStatement() {
        // 解析头部：CREATE TABLE 表名 (
        match(TokenType::KW_CREATE);
        match(TokenType::KW_TABLE);
        const Token& tableNameToken = match(TokenType::IDENTIFIER);
        match(TokenType::SYM_LPAREN); // 匹配左括号 '('

        auto node = std::make_unique<CreateTableASTNode>();
        node->tableName = tableNameToken.value;

        // 解析列定义：(列名 类型 [约束], ...)
        if (!check(TokenType::SYM_RPAREN)) {
            do {
                ColumnDefinition col;
                
                // 提取列名
                col.name = match(TokenType::IDENTIFIER).value;

                // 提取列的数据类型
                const Token& typeToken = consume();
                if (typeToken.type == TokenType::KW_INT || 
                    typeToken.type == TokenType::KW_CHAR || 
                    typeToken.type == TokenType::KW_VARCHAR) {
                    col.type = typeToken.value; // 保存类型名称
                } else {
                    throw std::runtime_error("Syntax Error: Invalid or missing data type for column '" + 
                                             col.name + "' at position " + std::to_string(typeToken.column));
                }

                // 提取后续可能的约束 (PRIMARY KEY, NOT NULL, UNIQUE)
                // 由于约束可能有多个且顺序不固定，采用 while 循环持续探查
                while (check(TokenType::KW_PRIMARY) || check(TokenType::KW_NOT) || check(TokenType::KW_UNIQUE)) {
                    if (check(TokenType::KW_PRIMARY)) {
                        match(TokenType::KW_PRIMARY);
                        match(TokenType::KW_KEY); // PRIMARY 后面必须紧跟 KEY
                        col.isPrimaryKey = true;
                    } 
                    else if (check(TokenType::KW_NOT)) {
                        match(TokenType::KW_NOT);
                        match(TokenType::KW_NULL); // NOT 后面必须紧跟 NULL
                        col.isNotNull = true;
                    } 
                    else if (check(TokenType::KW_UNIQUE)) {
                        match(TokenType::KW_UNIQUE);
                        col.isUnique = true;
                    }
                }

                // 将解析完毕的单列定义压入节点数组
                node->columns.push_back(col);

            // 逗号判决：遇到逗号说明还有下一列，循环继续；否则跳出
            } while (check(TokenType::SYM_COMMA) && (match(TokenType::SYM_COMMA), true)); 
        }

        // 解析尾部：) ;
        match(TokenType::SYM_RPAREN); // 匹配右括号 ')'
        match(TokenType::SYM_SEMICOLON);

        return node;
    }

    // 3.5.1 INSERT 语句解析逻辑
    // INSERT INTO <table_name> (<col1>, <col2>) VALUES (<val1>, <val2>);
    std::unique_ptr<ASTNode> SQLParserImpl::parseInsertStatement() {
        match(TokenType::KW_INSERT);
        match(TokenType::KW_INTO);
        
        auto node = std::make_unique<InsertASTNode>();
        node->tableName = match(TokenType::IDENTIFIER).value;

        // 解析列名列表 (可选)
        if (check(TokenType::SYM_LPAREN)) {
            match(TokenType::SYM_LPAREN);
            do {
                node->columns.push_back(match(TokenType::IDENTIFIER).value);
            } while (check(TokenType::SYM_COMMA) && (match(TokenType::SYM_COMMA), true));
            match(TokenType::SYM_RPAREN);
        }

        match(TokenType::KW_VALUES);
        match(TokenType::SYM_LPAREN);
        // 解析值列表
        do {
            const Token& val = peek();
            if (val.type == TokenType::NUMBER_LITERAL || val.type == TokenType::STRING_LITERAL) {
                // 直接调用 parsePrimary()，它会自动吞噬字面量并打包成 ASTNode 智能指针返回
                node->values.push_back(parsePrimary()); 
            } else {
                throw std::runtime_error("Syntax Error: Expected literal value in INSERT near '" + val.value + "'");
            }
        } while (check(TokenType::SYM_COMMA) && (match(TokenType::SYM_COMMA), true));
        match(TokenType::SYM_RPAREN);
        match(TokenType::SYM_SEMICOLON);

        return node;
    }

    // 3.5.3 SELECT 语句解析逻辑 (简化版 A 级需求)
    // SELECT <cols> FROM <table_name> WHERE <condition>;
    std::unique_ptr<ASTNode> SQLParserImpl::parseSelectStatement() {
        match(TokenType::KW_SELECT);
        auto node = std::make_unique<SelectASTNode>();

        // 解析查询字段
        if (check(TokenType::SYM_STAR)) {
            consume(); // 吞掉 *
            // 内部逻辑可以约定空列表代表 *
        } else {
            do {
                node->targetFields.push_back(parseExpression());
            } while (check(TokenType::SYM_COMMA) && (match(TokenType::SYM_COMMA), true));
        }

        match(TokenType::KW_FROM);
    
        // 物理逻辑：创建一个表节点，用来承载表名
        auto tableNode = std::make_unique<TableNode>();
        tableNode->tableName = match(TokenType::IDENTIFIER).value;
    
        // 将这个节点移动到 Select 节点的 fromSource 指针中
        node->fromSource = std::move(tableNode);

        // 解析 WHERE 子句
        if (check(TokenType::KW_WHERE)) {
            match(TokenType::KW_WHERE);
            node->whereExpressionTree = parseExpression();
        }

        match(TokenType::SYM_SEMICOLON);
        return node;
    }


    // 核心：表达式解析 (处理条件判断)
    // 采用优先级梯度：Expression -> Equality (=, !=) -> Comparison (>, <) -> Primary
    std::unique_ptr<ASTNode> SQLParserImpl::parseExpression() {
        return parseEquality();
    }

    std::unique_ptr<ASTNode> SQLParserImpl::parseEquality() {
        auto left = parseComparison();

        while (check(TokenType::SYM_EQ) || check(TokenType::SYM_NEQ)) {
            const Token& op = consume();
            auto right = parseComparison();
            
            auto newNode = std::make_unique<BinaryOperatorNode>();
            newNode->op = op.value;
            newNode->left = std::move(left);
            newNode->right = std::move(right);
            left = std::move(newNode);
        }
        return left;
    }

    std::unique_ptr<ASTNode> SQLParserImpl::parseComparison() {
        auto left = parsePrimary();

        while (check(TokenType::SYM_GT) || check(TokenType::SYM_GE) || 
               check(TokenType::SYM_LT) || check(TokenType::SYM_LE)) {
            const Token& op = consume();
            auto right = parsePrimary();
            
            auto newNode = std::make_unique<BinaryOperatorNode>();
            newNode->op = op.value;
            newNode->left = std::move(left);
            newNode->right = std::move(right);
            left = std::move(newNode);
        }
        return left;
    }

    std::unique_ptr<ASTNode> SQLParserImpl::parsePrimary() {
        const Token& token = peek();

        if (token.type == TokenType::NUMBER_LITERAL || token.type == TokenType::STRING_LITERAL) {
            auto node = std::make_unique<LiteralNode>();
            node->value = consume().value;
            return node;
        }

        if (token.type == TokenType::IDENTIFIER) {
            auto node = std::make_unique<ColumnRefNode>();
            node->columnName = consume().value;
            return node;
        }

        throw std::runtime_error("Syntax Error: Unexpected token in expression: " + token.value);
    }

} // namespace Parser