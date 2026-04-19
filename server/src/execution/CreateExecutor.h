#ifndef CREATE_EXECUTOR_H
#define CREATE_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
#include "../../include/parser/ASTNode.h"

namespace Execution {

    // 继承统一的 IExecutor 接口
    class CreateExecutor : public IExecutor {
    private:
        // 核心参数：执行器在创建时，必须把要干活的数据和工具“抱在怀里”
        Parser::ASTNode* astNode;          // 强转后的具体建表节点
        Catalog::ICatalogManager* catalog; // 户籍科工具，代表了整个数据库的“全局上下文状态”和“元数据持久化权限”。在 main.cpp 启动时，系统只会 new 出唯一的一个 CatalogManagerImpl 实例，所有的业务执行器都共享这个实例的地址。
        Storage::IStorageEngine* storage;  // 硬盘工具，代表了越过所有业务逻辑，直接对底层操作系统的硬盘文件进行二进制读写、绝对偏移量寻址以及文件目录管理的底层系统级权限。

    public:
        // 构造函数：由 ExecutorFactory 在实例化时传入这些依赖
        CreateExecutor(Parser::ASTNode* ast, 
                       Catalog::ICatalogManager* catalog, 
                       Storage::IStorageEngine* storage);
        
        ~CreateExecutor() override;

        // 重写核心执行接口
        bool execute() override;
    };

}
#endif // CREATE_EXECUTOR_H