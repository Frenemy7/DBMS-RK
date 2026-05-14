#ifndef I_OPERATOR_H
#define I_OPERATOR_H

#include "../../include/meta/TableMeta.h"
#include <vector>
#include <string>

namespace Execution {

    class IOperator {
    public:
        virtual ~IOperator() = default;

        // 初始化算子（例如打开文件游标，或者初始化排序缓冲区）
        virtual bool init() = 0;

        // 核心流水线方法：获取下一行数据
        // 参数 row：用于接收当前吐出的一行字符串数组
        // 返回值：如果还有数据则返回 true，如果到达 EOF 则返回 false
        virtual bool next(std::vector<std::string>& row) = 0;

        // 获取当前算子输出结果的表头结构（用于上下游算子之间的类型对齐）
        virtual std::vector<Meta::FieldBlock> getOutputSchema() const = 0;
    };

}

#endif // I_OPERATOR_H