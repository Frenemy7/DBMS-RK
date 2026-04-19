#ifndef IEXECUTOR_H
#define IEXECUTOR_H

namespace Execution {

    // 所有业务执行器的抽象基类
    class IExecutor {
    public:
        virtual ~IExecutor() = default;

        // 统一的执行入口。
        // 返回 true 表示执行成功，返回 false 表示执行失败（如主键冲突、类型不匹配等）。
        // 具体的错误信息可以通过全局的 ErrorCode 机制向外抛出。
        virtual bool execute() = 0;
    };

} // namespace Execution

#endif // IEXECUTOR_H