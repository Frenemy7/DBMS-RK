#include "ExecutorFactory.h"
#include "CreateExecutor.h"
#include "InsertExecutor.h"
#include "SelectExecutor.h"

#include <iostream>

namespace Execution {

    IExecutor* ExecutorFactory::createExecutor(
        Parser::ASTNode* ast, 
        Catalog::ICatalogManager* catalog, 
        Storage::IStorageEngine* storage) 
    {
        // TODO: 这个类的作用是作为中枢，将处理过的SQL语句，匹配到对应的实现上
    }

} // namespace Execution