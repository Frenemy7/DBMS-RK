#ifndef EXECUTOR_FACTORY_H
#define EXECUTOR_FACTORY_H

#include "../../include/parser/ASTNode.h"
#include "../../include/execution/IExecutor.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
#include "../../include/integrity/IIntegrityManager.h"
#include <memory>

namespace Execution {

    // 静态工厂类：负责根据 AST 节点类型动态分发执行器
    class ExecutorFactory {
    public:
        /**
         * 核心分发函数
         * @param ast 已经解析好的 AST 节点的所有权指针
         * @param catalog 元数据管理器的引用指针
         * @param storage 存储引擎的引用指针
         * @param integrity 完整性校验器的引用指针（用于 Insert/Update/Delete）
         * @return 对应的执行器唯一指针
         */
        static std::unique_ptr<IExecutor> createExecutor(
            std::unique_ptr<Parser::ASTNode> ast, 
            Catalog::ICatalogManager* catalog, 
            Storage::IStorageEngine* storage,
            Integrity::IIntegrityManager* integrity
        );
    };

} // namespace Execution

#endif // EXECUTOR_FACTORY_H