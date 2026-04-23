#ifndef SELECT_AST_NODE_H
#define SELECT_AST_NODE_H

#include "ASTNode.h"
#include <string>
#include <vector>

namespace Parser {

    struct OrderByItem {
        std::string field; // 排序的字段名
        bool isAsc;        // 存排序方向。true 升序，false 降序
    };

    // Select 节点 
    //（以[SELECT], [A.name], [,], [COUNT(B.score)], [FROM], [student A], [JOIN], [score B], [ON], [A.id = B.uid] 为例）
    class SelectASTNode : public ASTNode {
    public:
        SQLType getType() const override { return SQLType::SELECT; }

        // 是否去重 SELECT DISTINCT
        bool isDistinct = false;

        // 子节点：

        // 查询目标（SELECT 之后，FROM 之前）即 A.name 和 COUNT(B.score) 采用vector存储多列目标，数据类型为指向基类的一个C++指针。选择基类指针是因为选择对象可能为聚合函数之类的东西
        std::vector<std::unique_ptr<ASTNode>> targetFields;

        // 数据源（FROM 之后，WHERE 之前）即 from 后面的东西
        std::unique_ptr<ASTNode> fromSource;

        // 过滤条件（WHERE 之后）
        std::unique_ptr<ASTNode> whereExpressionTree; 

        // 分组依据 (GROUP BY 后面的列或表达式列表)
        std::vector<std::unique_ptr<ASTNode>> groupByFields;

        // 分组后的二次过滤 (HAVING 之后的部分)
        std::unique_ptr<ASTNode> havingExpressionTree;

        // 排序规则 (ORDER BY)
        std::vector<OrderByItem> orderByItems;
    };

    
    
}
#endif // SELECT_AST_NODE_H