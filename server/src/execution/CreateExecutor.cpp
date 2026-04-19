#include "CreateExecutor.h"
#include <iostream>

namespace Execution {

    // 构造函数：接收工厂扔过来的参数并保存
    CreateExecutor::CreateExecutor(Parser::ASTNode* ast, 
                                   Catalog::ICatalogManager* cat, 
                                   Storage::IStorageEngine* stor) 
        : astNode(ast), catalog(cat), storage(stor) {
    }

    CreateExecutor::~CreateExecutor() {}

    bool CreateExecutor::execute() {
        

    
        
        return false; // 占位
        /*
        我的思路是这样的：
        举例：CREATE TABLE Employee (emp_id INTEGER, name VARCHAR(64), salary DOUBLE);

        1.首先，函数外部调用语句解析函数，然后将处理结果传给factory，factory匹配到对应的实现类

        2.接下来，就是这个类的具体实现。首先是文件的合法性检验，然后，建表，
        */
    }

} // namespace Execution