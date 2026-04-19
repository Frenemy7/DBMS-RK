#ifndef EXECUTOR_FACTORY_H
#define EXECUTOR_FACTORY_H

#include "../../include/parser/ASTNode.h"
#include "../../include/execution/IExecutor.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"

namespace Execution {

    // 这是一个静态工厂类，不需要被实例化，直接用 类名::方法名 调用
    class ExecutorFactory {
    public:
        // 核心生产线：根据传入的 ast 节点类型，生产出对应的执行器
        static IExecutor* createExecutor(
            Parser::ASTNode* ast, 
            Catalog::ICatalogManager* catalog, 
            Storage::IStorageEngine* storage
        );
    };

}

#endif // EXECUTOR_FACTORY_H