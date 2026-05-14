#include "SQLParserImpl.h"
#include "../../include/parser/DropTableASTNode.h"
#include "../../include/parser/CreateTableASTNode.h"
#include "../../include/parser/InsertASTNode.h"
#include "../../include/parser/UpdateASTNode.h"
#include "../../include/parser/SelectASTNode.h"
#include <iostream>
#include <stdexcept>

namespace Parser {

    // 构造函数：初始化游标位置
    SQLParserImpl::SQLParserImpl() : currentTokenIndex(0) {}

    std::unique_ptr<ASTNode> SQLParserImpl::parse(const std::string& sql) {
        Lexer lexer(sql);
        tokens = lexer.getTokens();
        currentTokenIndex = 0;
        if (isAtEnd()) return nullptr;
        const Token& firstToken = peek();
        switch (firstToken.type) {
            case TokenType::KW_DROP:
                if (currentTokenIndex + 1 < (int)tokens.size()) {
                    if (tokens[currentTokenIndex + 1].type == TokenType::KW_USER)
                        return parseDropUserStatement();
                    if (tokens[currentTokenIndex + 1].type == TokenType::KW_DATABASE)
                        return parseDropDatabaseStatement();
                }
                return parseDropTableStatement();
            case TokenType::KW_CREATE:
                if (currentTokenIndex + 1 < tokens.size()) {
                    if (tokens[currentTokenIndex + 1].type == TokenType::KW_USER)
                        return parseCreateUserStatement();
                    std::string nextVal = tokens[currentTokenIndex + 1].value;
                    if (tokens[currentTokenIndex + 1].type == TokenType::KW_DATABASE || nextVal == "DATABASE" || nextVal == "database")
                        return parseCreateDatabaseStatement();
                }
                return parseCreateTableStatement();
            case TokenType::KW_USE: return parseUseDatabaseStatement();
            case TokenType::KW_GRANT:  return parseGrantRevokeStatement();
            case TokenType::KW_REVOKE: return parseGrantRevokeStatement();
            case TokenType::KW_BACKUP: return parseBackupDatabaseStatement();
            case TokenType::KW_RESTORE:return parseRestoreDatabaseStatement();
            case TokenType::KW_ALTER:  return parseAlterTableStatement();
            case TokenType::KW_UPDATE: return parseUpdateStatement();
            case TokenType::KW_INSERT: return parseInsertStatement();
            case TokenType::KW_SELECT: return parseSelectStatement();
            default:
                if (firstToken.type == TokenType::IDENTIFIER && (firstToken.value == "USE" || firstToken.value == "use"))
                    return parseUseDatabaseStatement();
                throw std::runtime_error("Syntax Error: Unsupported SQL statement starting with '" +
                                         firstToken.value + "' at position " + std::to_string(firstToken.column));
        }
    }

    bool SQLParserImpl::isAtEnd() const { return peek().type == TokenType::END_OF_FILE; }
    const Token& SQLParserImpl::peek() const { return tokens[currentTokenIndex]; }
    const Token& SQLParserImpl::previous() const { return tokens[currentTokenIndex - 1]; }
    const Token& SQLParserImpl::consume() { if (!isAtEnd()) currentTokenIndex++; return previous(); }
    bool SQLParserImpl::check(TokenType type) const { if (isAtEnd()) return false; return peek().type == type; }
    const Token& SQLParserImpl::match(TokenType expected) {
        if (check(expected)) return consume();
        throw std::runtime_error("Syntax Error: Expected different token near '" +
                                 peek().value + "' at position " + std::to_string(peek().column));
    }

    // ============================================================
    //  DROP TABLE 语句解析
    // ============================================================
    std::unique_ptr<ASTNode> SQLParserImpl::parseDropTableStatement() {
        match(TokenType::KW_DROP); match(TokenType::KW_TABLE);
        const Token& tableNameToken = match(TokenType::IDENTIFIER);
        match(TokenType::SYM_SEMICOLON);
        auto node = std::make_unique<DropTableASTNode>();
        node->tableName = tableNameToken.value;
        return node;
    }

    // ============================================================
    //  CREATE TABLE 语句解析
    // ============================================================
    std::unique_ptr<ASTNode> SQLParserImpl::parseCreateTableStatement() {
        match(TokenType::KW_CREATE); match(TokenType::KW_TABLE);
        const Token& tableNameToken = match(TokenType::IDENTIFIER);
        match(TokenType::SYM_LPAREN);
        auto node = std::make_unique<CreateTableASTNode>();
        node->tableName = tableNameToken.value;
        if (!check(TokenType::SYM_RPAREN)) {
            do {
                ColumnDefinition col;
                col.name = match(TokenType::IDENTIFIER).value;
                const Token& typeToken = consume();
                if (typeToken.type == TokenType::KW_INT || typeToken.type == TokenType::KW_CHAR ||
                    typeToken.type == TokenType::KW_VARCHAR || typeToken.type == TokenType::KW_DOUBLE ||
                    typeToken.type == TokenType::KW_DATETIME) {
                    col.type = typeToken.value;
                    if (check(TokenType::SYM_LPAREN)) {
                        match(TokenType::SYM_LPAREN);
                        col.param = std::stoi(match(TokenType::NUMBER_LITERAL).value);
                        match(TokenType::SYM_RPAREN);
                    }
                } else {
                    throw std::runtime_error("Syntax Error: Invalid data type for column '" + col.name + "'");
                }
                while (check(TokenType::KW_PRIMARY) || check(TokenType::KW_NOT) || check(TokenType::KW_UNIQUE) || check(TokenType::KW_DEFAULT) || check(TokenType::KW_CHECK) || check(TokenType::KW_IDENTITY)) {
                    if (check(TokenType::KW_PRIMARY)) {
                        if (col.isPrimaryKey) throw std::runtime_error("Syntax Error: duplicate PRIMARY KEY on '" + col.name + "'");
                        match(TokenType::KW_PRIMARY); match(TokenType::KW_KEY); col.isPrimaryKey = true;
                    } else if (check(TokenType::KW_NOT)) {
                        if (col.isNotNull) throw std::runtime_error("Syntax Error: duplicate NOT NULL on '" + col.name + "'");
                        match(TokenType::KW_NOT); match(TokenType::KW_NULL); col.isNotNull = true;
                    } else if (check(TokenType::KW_UNIQUE)) {
                        if (col.isUnique) throw std::runtime_error("Syntax Error: duplicate UNIQUE on '" + col.name + "'");
                        match(TokenType::KW_UNIQUE); col.isUnique = true;
                    } else if (check(TokenType::KW_DEFAULT)) {
                        if (col.hasDefault) throw std::runtime_error("Syntax Error: duplicate DEFAULT on '" + col.name + "'");
                        match(TokenType::KW_DEFAULT);
                        // 默认值：数字字面量或字符串字面量
                        if (check(TokenType::NUMBER_LITERAL)) {
                            col.defaultValue = consume().value;
                        } else if (check(TokenType::STRING_LITERAL)) {
                            col.defaultValue = consume().value;
                        } else if (check(TokenType::KW_NULL)) {
                            consume(); col.defaultValue = "NULL";
                        } else {
                            throw std::runtime_error("Syntax Error: DEFAULT 需要字面量默认值");
                        }
                        col.hasDefault = true;
                    } else if (check(TokenType::KW_CHECK)) {
                        if (col.hasCheck) throw std::runtime_error("Syntax Error: duplicate CHECK on '" + col.name + "'");
                        match(TokenType::KW_CHECK);
                        match(TokenType::SYM_LPAREN);
                        // 将 CHECK 条件提取为纯文本（到匹配的右括号为止）
                        std::string cond;
                        int depth = 1;
                        while (depth > 0 && !isAtEnd()) {
                            const Token& t = consume();
                            if (t.type == TokenType::SYM_LPAREN) depth++;
                            else if (t.type == TokenType::SYM_RPAREN) { depth--; if (depth == 0) break; }
                            cond += t.value;
                        }
                        if (depth > 0) throw std::runtime_error("Syntax Error: CHECK 缺少闭合括号");
                        col.checkCondition = cond;
                        col.hasCheck = true;
                    } else if (check(TokenType::KW_IDENTITY)) {
                        if (col.isIdentity) throw std::runtime_error("Syntax Error: duplicate IDENTITY on '" + col.name + "'");
                        match(TokenType::KW_IDENTITY); col.isIdentity = true;
                    }
                }
                node->columns.push_back(col);
            } while (check(TokenType::SYM_COMMA) && (match(TokenType::SYM_COMMA), true));
        }
        match(TokenType::SYM_RPAREN); match(TokenType::SYM_SEMICOLON);
        return node;
    }

    // ============================================================
    //  ALTER TABLE 语句解析
    //  ALTER TABLE tablename ADD COLUMN colname type(params);
    //  ALTER TABLE tablename DROP COLUMN colname;
    //  ALTER TABLE tablename MODIFY COLUMN colname type(params);
    // ============================================================
    std::unique_ptr<ASTNode> SQLParserImpl::parseAlterTableStatement() {
        match(TokenType::KW_ALTER); match(TokenType::KW_TABLE);
        auto node = std::make_unique<AlterTableASTNode>();
        node->tableName = match(TokenType::IDENTIFIER).value;

        // ADD / DROP / MODIFY
        if (check(TokenType::KW_ADD)) {
            consume(); node->alterOp = "ADD";
        } else if (check(TokenType::KW_DROP)) {
            consume(); node->alterOp = "DROP";
        } else if (check(TokenType::KW_MODIFY)) {
            consume(); node->alterOp = "MODIFY";
        } else {
            throw std::runtime_error("Syntax Error: ALTER TABLE 需要 ADD、DROP 或 MODIFY。");
        }

        // 可选的 COLUMN 关键字
        if (check(TokenType::KW_COLUMN)) consume();

        node->columnName = match(TokenType::IDENTIFIER).value;

        // ADD / MODIFY 需要类型定义
        if (node->alterOp == "ADD" || node->alterOp == "MODIFY") {
            const Token& typeToken = consume();
            if (typeToken.type == TokenType::KW_INT || typeToken.type == TokenType::KW_CHAR ||
                typeToken.type == TokenType::KW_VARCHAR || typeToken.type == TokenType::KW_DOUBLE ||
                typeToken.type == TokenType::KW_DATETIME) {
                node->columnType = typeToken.value;
                if (check(TokenType::SYM_LPAREN)) {
                    match(TokenType::SYM_LPAREN);
                    node->columnParam = std::stoi(match(TokenType::NUMBER_LITERAL).value);
                    match(TokenType::SYM_RPAREN);
                }
            } else {
                throw std::runtime_error("Syntax Error: ALTER TABLE 需要合法的数据类型。");
            }
        }

        match(TokenType::SYM_SEMICOLON);
        return node;
    }

    // ============================================================
    //  UPDATE 语句解析
    //  UPDATE tablename SET col = expr, ... WHERE condition;
    // ============================================================
    std::unique_ptr<ASTNode> SQLParserImpl::parseUpdateStatement() {
        match(TokenType::KW_UPDATE);
        auto node = std::make_unique<UpdateASTNode>();
        node->tableName = match(TokenType::IDENTIFIER).value;
        match(TokenType::KW_SET);
        do {
            SetClause sc;
            sc.field = match(TokenType::IDENTIFIER).value;
            match(TokenType::SYM_EQ);
            sc.valueExpr = parseExpression(); // 支持 grade * 1.05 等表达式
            node->setValues.push_back(std::move(sc));
        } while (check(TokenType::SYM_COMMA) && (consume(), true));
        if (check(TokenType::KW_WHERE)) { consume(); node->whereExpressionTree = parseOr(); }
        match(TokenType::SYM_SEMICOLON);
        return node;
    }

    // ============================================================
    //  INSERT 语句解析（支持 VALUES 和 SELECT 两种形式）
    // ============================================================
    std::unique_ptr<ASTNode> SQLParserImpl::parseInsertStatement() {
        match(TokenType::KW_INSERT); match(TokenType::KW_INTO);
        auto node = std::make_unique<InsertASTNode>();
        node->tableName = match(TokenType::IDENTIFIER).value;
        if (check(TokenType::SYM_LPAREN)) {
            match(TokenType::SYM_LPAREN);
            do { node->columns.push_back(match(TokenType::IDENTIFIER).value); }
            while (check(TokenType::SYM_COMMA) && (match(TokenType::SYM_COMMA), true));
            match(TokenType::SYM_RPAREN);
        }
        // INSERT ... SELECT ...
        if (check(TokenType::KW_SELECT)) {
            node->selectSource = parseSelectStatement();
            return node;
        }
        // INSERT ... VALUES (...)
        match(TokenType::KW_VALUES); match(TokenType::SYM_LPAREN);
        do {
            const Token& val = peek();
            if (val.type == TokenType::NUMBER_LITERAL || val.type == TokenType::STRING_LITERAL)
                node->values.push_back(parsePrimary());
            else throw std::runtime_error("Syntax Error: Expected literal in INSERT near '" + val.value + "'");
        } while (check(TokenType::SYM_COMMA) && (match(TokenType::SYM_COMMA), true));
        match(TokenType::SYM_RPAREN); match(TokenType::SYM_SEMICOLON);
        return node;
    }

    // ============================================================
    //  SELECT 语句解析（完整版：JOIN / GROUP BY / HAVING / ORDER BY / 子查询 / 算术）
    // ============================================================
    std::unique_ptr<ASTNode> SQLParserImpl::parseSelectStatement() {
        match(TokenType::KW_SELECT);
        auto node = std::make_unique<SelectASTNode>();

        // DISTINCT
        if (check(TokenType::KW_DISTINCT)) { consume(); node->isDistinct = true; }

        // 目标列列表
        if (check(TokenType::SYM_STAR)) {
            auto col = std::make_unique<ColumnRefNode>(); col->columnName = "*";
            node->targetFields.push_back(std::move(col)); consume();
        } else {
            do { node->targetFields.push_back(parseSelectItem()); }
            while (check(TokenType::SYM_COMMA) && (consume(), true));
        }

        // FROM 子句（支持 JOIN）
        match(TokenType::KW_FROM);
        node->fromSource = parseTableSource();

        // WHERE
        if (check(TokenType::KW_WHERE)) { consume(); node->whereExpressionTree = parseOr(); }

        // GROUP BY
        if (check(TokenType::KW_GROUP)) {
            consume(); match(TokenType::KW_BY);
            do { node->groupByFields.push_back(parseExpression()); }
            while (check(TokenType::SYM_COMMA) && (consume(), true));
        }

        // HAVING
        if (check(TokenType::KW_HAVING)) { consume(); node->havingExpressionTree = parseOr(); }

        // ORDER BY
        if (check(TokenType::KW_ORDER)) {
            consume(); match(TokenType::KW_BY);
            do { node->orderByItems.push_back(parseOrderByItem()); }
            while (check(TokenType::SYM_COMMA) && (consume(), true));
        }

        // 顶层查询需要分号；子查询内部以 ) 结束则跳过
        if (!check(TokenType::SYM_RPAREN)) match(TokenType::SYM_SEMICOLON);
        return node;
    }

    // SELECT 列表单项：表达式 [AS] 别名
    std::unique_ptr<ASTNode> SQLParserImpl::parseSelectItem() {
        auto expr = parseExpression();
        if (check(TokenType::KW_AS)) {
            consume();
            auto alias = std::make_unique<BinaryOperatorNode>(); alias->op = "AS";
            alias->left = std::move(expr);
            auto name = std::make_unique<ColumnRefNode>(); name->columnName = match(TokenType::IDENTIFIER).value;
            alias->right = std::move(name);
            return alias;
        }
        if (check(TokenType::IDENTIFIER)) {
            auto alias = std::make_unique<BinaryOperatorNode>(); alias->op = "AS";
            alias->left = std::move(expr);
            auto name = std::make_unique<ColumnRefNode>(); name->columnName = consume().value;
            alias->right = std::move(name);
            return alias;
        }
        return expr;
    }

    // 表源：支持 JOIN 链式和逗号隐式 CROSS JOIN
    std::unique_ptr<ASTNode> SQLParserImpl::parseTableSource() {
        auto left = parseTableRef();
        while (true) {
            std::string joinType;
            if (check(TokenType::KW_LEFT)) { consume(); match(TokenType::KW_JOIN); joinType = "LEFT"; }
            else if (check(TokenType::KW_RIGHT)) { consume(); match(TokenType::KW_JOIN); joinType = "RIGHT"; }
            else if (check(TokenType::KW_INNER)) { consume(); joinType = "INNER"; if (check(TokenType::KW_JOIN)) consume(); }
            else if (check(TokenType::KW_CROSS)) { consume(); joinType = "CROSS"; if (check(TokenType::KW_JOIN)) consume(); }
            else if (check(TokenType::KW_FULL)) { consume(); joinType = "FULL"; if (check(TokenType::KW_JOIN)) consume(); }
            else if (check(TokenType::KW_JOIN)) { consume(); joinType = "INNER"; }
            else if (check(TokenType::SYM_COMMA)) { consume(); joinType = "CROSS"; }
            else break;
            auto join = std::make_unique<JoinNode>();
            join->left = std::move(left); join->joinType = joinType;
            join->right = parseTableRef();
            if (joinType != "CROSS") { match(TokenType::KW_ON); join->onCondition = parseOr(); }
            left = std::move(join);
        }
        return left;
    }

    // 单个表引用：表名 [别名] 或 (子查询) 别名
    std::unique_ptr<ASTNode> SQLParserImpl::parseTableRef() {
        if (check(TokenType::SYM_LPAREN)) {
            consume();
            if (check(TokenType::KW_SELECT)) {
                auto sub = std::make_unique<SubqueryNode>();
                sub->subquery = parseSelectStatement();
                match(TokenType::SYM_RPAREN);
                if (check(TokenType::KW_AS)) consume();
                std::string alias = match(TokenType::IDENTIFIER).value;
                auto wrap = std::make_unique<JoinNode>(); wrap->joinType = "SUBQUERY";
                wrap->left = std::move(sub);
                auto aliasNode = std::make_unique<TableNode>();
                aliasNode->tableName = alias; aliasNode->alias = alias;
                wrap->right = std::move(aliasNode);
                return wrap;
            }
            throw std::runtime_error("Syntax Error: Expected subquery in FROM");
        }
        auto table = std::make_unique<TableNode>();
        table->tableName = match(TokenType::IDENTIFIER).value;
        if (check(TokenType::KW_AS)) consume();
        if (check(TokenType::IDENTIFIER)) table->alias = consume().value;
        return table;
    }

    // ORDER BY 单项
    OrderByItem SQLParserImpl::parseOrderByItem() {
        auto expr = parseExpression(); bool asc = true;
        if (check(TokenType::KW_ASC)) consume();
        else if (check(TokenType::KW_DESC)) { consume(); asc = false; }
        OrderByItem item; item.isAsc = asc;
        if (auto col = dynamic_cast<ColumnRefNode*>(expr.get())) item.field = col->columnName;
        return item;
    }

    // ============================================================
    //  表达式解析：递归下降 + 运算符优先级
    //  优先级链：OR → AND → NOT → +,- → *,/ → 比较 → Primary
    // ============================================================
    std::unique_ptr<ASTNode> SQLParserImpl::parseExpression() { return parseOr(); }

    std::unique_ptr<ASTNode> SQLParserImpl::parseOr() {
        auto left = parseAnd();
        while (check(TokenType::KW_OR)) {
            auto op = consume(); auto right = parseAnd();
            auto bin = std::make_unique<BinaryOperatorNode>(); bin->op = op.value;
            bin->left = std::move(left); bin->right = std::move(right); left = std::move(bin);
        }
        return left;
    }

    std::unique_ptr<ASTNode> SQLParserImpl::parseAnd() {
        auto left = parseNot();
        while (check(TokenType::KW_AND)) {
            auto op = consume(); auto right = parseNot();
            auto bin = std::make_unique<BinaryOperatorNode>(); bin->op = op.value;
            bin->left = std::move(left); bin->right = std::move(right); left = std::move(bin);
        }
        return left;
    }

    // NOT 取反
    std::unique_ptr<ASTNode> SQLParserImpl::parseNot() {
        if (check(TokenType::KW_NOT)) {
            consume();
            auto expr = parseAddSub();
            auto notNode = std::make_unique<BinaryOperatorNode>(); notNode->op = "NOT";
            notNode->left = std::move(expr); return notNode;
        }
        return parseAddSub();
    }

    // 加减运算：+ , -
    std::unique_ptr<ASTNode> SQLParserImpl::parseAddSub() {
        auto left = parseMulDiv();
        while (check(TokenType::SYM_PLUS) || check(TokenType::SYM_MINUS)) {
            auto op = consume(); auto right = parseMulDiv();
            auto bin = std::make_unique<BinaryOperatorNode>(); bin->op = op.value;
            bin->left = std::move(left); bin->right = std::move(right); left = std::move(bin);
        }
        return left;
    }

    // 乘除运算：* , /
    std::unique_ptr<ASTNode> SQLParserImpl::parseMulDiv() {
        auto left = parseComparison();
        while (check(TokenType::SYM_STAR) || check(TokenType::SYM_SLASH)) {
            auto op = consume(); auto right = parseComparison();
            auto bin = std::make_unique<BinaryOperatorNode>(); bin->op = op.value;
            bin->left = std::move(left); bin->right = std::move(right); left = std::move(bin);
        }
        return left;
    }

    // 比较运算：= != > < >= <=  LIKE  IS NULL  IN  BETWEEN
    std::unique_ptr<ASTNode> SQLParserImpl::parseComparison() {
        auto left = parsePrimary();

        // 基础比较运算符
        if (check(TokenType::SYM_EQ) || check(TokenType::SYM_NEQ) || check(TokenType::SYM_GT) ||
            check(TokenType::SYM_LT) || check(TokenType::SYM_GE) || check(TokenType::SYM_LE)) {
            auto op = consume(); auto right = parsePrimary();
            auto bin = std::make_unique<BinaryOperatorNode>(); bin->op = op.value;
            bin->left = std::move(left); bin->right = std::move(right); return bin;
        }

        // LIKE 模糊匹配
        if (check(TokenType::KW_LIKE)) {
            consume();
            auto bin = std::make_unique<BinaryOperatorNode>(); bin->op = "LIKE";
            bin->left = std::move(left); bin->right = parsePrimary(); return bin;
        }

        // IS [NOT] NULL
        if (check(TokenType::KW_IS)) {
            consume();
            bool nn = false; if (check(TokenType::KW_NOT)) { consume(); nn = true; }
            match(TokenType::KW_NULL);
            auto n = std::make_unique<IsNullNode>(); n->expr = std::move(left); n->notNull = nn; return n;
        }

        // [NOT] IN (子查询 | 值列表)
        if (check(TokenType::KW_IN) || (check(TokenType::KW_NOT) && currentTokenIndex + 1 < (int)tokens.size() && tokens[currentTokenIndex + 1].type == TokenType::KW_IN)) {
            bool ni = false; if (check(TokenType::KW_NOT)) { consume(); ni = true; }
            match(TokenType::KW_IN); match(TokenType::SYM_LPAREN);
            if (check(TokenType::KW_SELECT)) {
                auto sub = std::make_unique<SubqueryNode>(); sub->subquery = parseSelectStatement(); match(TokenType::SYM_RPAREN);
                auto bin = std::make_unique<BinaryOperatorNode>(); bin->op = ni ? "NOT IN" : "IN";
                bin->left = std::move(left); bin->right = std::move(sub); return bin;
            }
            auto inList = std::make_unique<InListNode>();
            do { inList->items.push_back(parsePrimary()); } while (check(TokenType::SYM_COMMA) && (consume(), true));
            match(TokenType::SYM_RPAREN);
            auto bin = std::make_unique<BinaryOperatorNode>(); bin->op = ni ? "NOT IN" : "IN";
            bin->left = std::move(left); bin->right = std::move(inList); return bin;
        }

        // [NOT] BETWEEN x AND y
        if (check(TokenType::KW_BETWEEN) || (check(TokenType::KW_NOT) && currentTokenIndex + 1 < (int)tokens.size() && tokens[currentTokenIndex + 1].type == TokenType::KW_BETWEEN)) {
            bool nb = false; if (check(TokenType::KW_NOT)) { consume(); nb = true; }
            match(TokenType::KW_BETWEEN); auto low = parsePrimary(); match(TokenType::KW_AND); auto high = parsePrimary();
            auto between = std::make_unique<BetweenNode>();
            between->expr = std::move(left); between->low = std::move(low); between->high = std::move(high);
            if (nb) {
                auto bin = std::make_unique<BinaryOperatorNode>(); bin->op = "NOT_BETWEEN";
                bin->left = std::move(between); return bin;
            }
            return between;
        }

        return left;
    }

    // 最小单元：字面量、EXISTS、括号表达式/子查询、列引用、函数调用
    std::unique_ptr<ASTNode> SQLParserImpl::parsePrimary() {
        const Token& token = peek();

        // 字面量：数字或字符串
        if (token.type == TokenType::NUMBER_LITERAL || token.type == TokenType::STRING_LITERAL) {
            auto node = std::make_unique<LiteralNode>(); node->value = consume().value; return node;
        }

        // EXISTS (子查询)
        if (token.type == TokenType::KW_EXISTS) {
            consume(); match(TokenType::SYM_LPAREN);
            auto sub = std::make_unique<SubqueryNode>(); sub->subquery = parseSelectStatement(); match(TokenType::SYM_RPAREN);
            auto bin = std::make_unique<BinaryOperatorNode>(); bin->op = "EXISTS"; bin->left = std::move(sub); return bin;
        }

        // 括号：(表达式) 或 (子查询)
        if (check(TokenType::SYM_LPAREN)) {
            consume();
            if (check(TokenType::KW_SELECT)) {
                auto sub = std::make_unique<SubqueryNode>(); sub->subquery = parseSelectStatement(); match(TokenType::SYM_RPAREN); return sub;
            }
            auto expr = parseOr(); match(TokenType::SYM_RPAREN); return expr;
        }

        // 标识符（点号引用 table.column）、聚合函数（COUNT/SUM/AVG/MAX/MIN）
        if (token.type == TokenType::IDENTIFIER || token.type == TokenType::KW_COUNT || token.type == TokenType::KW_SUM ||
            token.type == TokenType::KW_AVG || token.type == TokenType::KW_MAX || token.type == TokenType::KW_MIN) {
            // 后跟 '(' → 函数调用
            if (currentTokenIndex + 1 < (int)tokens.size() && tokens[currentTokenIndex + 1].type == TokenType::SYM_LPAREN)
                return parseFunctionCall();

            std::string name = consume().value;

            // 点号：table.column
            if (check(TokenType::SYM_DOT)) {
                consume();
                std::string cn = check(TokenType::SYM_STAR) ? (consume(), "*") : match(TokenType::IDENTIFIER).value;
                auto col = std::make_unique<ColumnRefNode>(); col->tableName = name; col->columnName = cn; return col;
            }

            // 简单列引用
            auto col = std::make_unique<ColumnRefNode>(); col->columnName = name; return col;
        }

        throw std::runtime_error("Syntax Error: Unexpected token in expression: " + token.value);
    }

    // 函数调用：COUNT(*), SUM(col), AVG(DISTINCT col) 等
    std::unique_ptr<ASTNode> SQLParserImpl::parseFunctionCall() {
        auto func = std::make_unique<FunctionCallNode>();
        func->functionName = consume().value; match(TokenType::SYM_LPAREN);
        if (check(TokenType::KW_DISTINCT)) { consume(); func->isDistinct = true; }
        if (check(TokenType::SYM_STAR)) consume(); // COUNT(*)
        else if (!check(TokenType::SYM_RPAREN)) {
            do { func->arguments.push_back(parseExpression()); } while (check(TokenType::SYM_COMMA) && (consume(), true));
        }
        match(TokenType::SYM_RPAREN); return func;
    }

    // ============================================================
    //  DROP DATABASE
    // ============================================================
    std::unique_ptr<ASTNode> SQLParserImpl::parseDropDatabaseStatement() {
        match(TokenType::KW_DROP);
        if (check(TokenType::KW_DATABASE)) consume();
        else match(TokenType::IDENTIFIER); // 兜底："DATABASE" 被当成标识符
        const Token& dbNameToken = match(TokenType::IDENTIFIER);
        if (!check(TokenType::SYM_SEMICOLON) && !isAtEnd())
            throw std::runtime_error("Syntax Error: Unexpected token near database name");
        if (check(TokenType::SYM_SEMICOLON)) consume();
        auto node = std::make_unique<DropDatabaseASTNode>();
        node->dbName = dbNameToken.value;
        return node;
    }

    // ============================================================
    //  BACKUP / RESTORE DATABASE
    // ============================================================
    std::unique_ptr<ASTNode> SQLParserImpl::parseBackupDatabaseStatement() {
        match(TokenType::KW_BACKUP);
        if (check(TokenType::KW_DATABASE)) consume();
        auto node = std::make_unique<BackupDatabaseASTNode>();
        node->dbName = match(TokenType::IDENTIFIER).value;
        if (check(TokenType::KW_TO)) consume();
        node->path = match(TokenType::STRING_LITERAL).value;
        if (check(TokenType::SYM_SEMICOLON)) consume();
        return node;
    }

    std::unique_ptr<ASTNode> SQLParserImpl::parseRestoreDatabaseStatement() {
        match(TokenType::KW_RESTORE);
        if (check(TokenType::KW_DATABASE)) consume();
        auto node = std::make_unique<RestoreDatabaseASTNode>();
        node->dbName = match(TokenType::IDENTIFIER).value;
        if (check(TokenType::KW_FROM)) consume();
        node->path = match(TokenType::STRING_LITERAL).value;
        if (check(TokenType::SYM_SEMICOLON)) consume();
        return node;
    }

    // ============================================================
    //  用户管理
    // ============================================================
    std::unique_ptr<ASTNode> SQLParserImpl::parseCreateUserStatement() {
        match(TokenType::KW_CREATE); match(TokenType::KW_USER);
        auto node = std::make_unique<CreateUserASTNode>();
        node->username = match(TokenType::IDENTIFIER).value;
        match(TokenType::KW_IDENTIFIED); match(TokenType::KW_BY);
        node->password = match(TokenType::STRING_LITERAL).value;
        if (check(TokenType::SYM_SEMICOLON)) consume();
        return node;
    }

    std::unique_ptr<ASTNode> SQLParserImpl::parseDropUserStatement() {
        match(TokenType::KW_DROP); match(TokenType::KW_USER);
        auto node = std::make_unique<DropUserASTNode>();
        node->username = match(TokenType::IDENTIFIER).value;
        if (check(TokenType::SYM_SEMICOLON)) consume();
        return node;
    }

    std::unique_ptr<ASTNode> SQLParserImpl::parseGrantRevokeStatement() {
        std::string mode = check(TokenType::KW_GRANT) ? "GRANT" : "REVOKE";
        consume();
        auto node = std::make_unique<GrantRevokeASTNode>();
        node->mode = mode;

        if (mode == "GRANT") {
            // GRANT <role> <user>;
            node->role = match(TokenType::IDENTIFIER).value;
            node->username = match(TokenType::IDENTIFIER).value;
        } else {
            // REVOKE [role FROM] <user>;  或 REVOKE <user>;
            if (currentTokenIndex + 1 < (int)tokens.size() &&
                tokens[currentTokenIndex + 1].type == TokenType::KW_FROM) {
                node->role = match(TokenType::IDENTIFIER).value;
                match(TokenType::KW_FROM);
            } else {
                node->role = "";
            }
            node->username = match(TokenType::IDENTIFIER).value;
        }
        if (check(TokenType::SYM_SEMICOLON)) consume();
        return node;
    }

    // ============================================================
    //  CREATE / USE DATABASE
    // ============================================================
    std::unique_ptr<ASTNode> SQLParserImpl::parseCreateDatabaseStatement() {
        match(TokenType::KW_CREATE);
        if (check(TokenType::KW_DATABASE)) consume(); else match(TokenType::IDENTIFIER);
        const Token& dbNameToken = match(TokenType::IDENTIFIER);
        if (!check(TokenType::SYM_SEMICOLON) && !isAtEnd())
            throw std::runtime_error("Syntax Error: Unexpected token near database name");
        if (check(TokenType::SYM_SEMICOLON)) consume();
        auto node = std::make_unique<CreateDatabaseASTNode>(); node->dbName = dbNameToken.value; return node;
    }

    std::unique_ptr<ASTNode> SQLParserImpl::parseUseDatabaseStatement() {
        if (check(TokenType::KW_USE)) consume(); else match(TokenType::IDENTIFIER);
        if (check(TokenType::KW_DATABASE)) consume();
        else if (check(TokenType::IDENTIFIER) && (peek().value == "DATABASE" || peek().value == "database")) consume();
        const Token& dbNameToken = match(TokenType::IDENTIFIER);
        if (check(TokenType::SYM_SEMICOLON)) consume();
        auto node = std::make_unique<UseDatabaseASTNode>(); node->dbName = dbNameToken.value; return node;
    }

} // namespace Parser
