#include "UpdateExecutor.h"
#include <iostream>

namespace Execution {

    UpdateExecutor::UpdateExecutor(std::unique_ptr<Parser::UpdateASTNode> ast, 
                                   Catalog::ICatalogManager* cat, 
                                   Storage::IStorageEngine* stor,
                                   Integrity::IIntegrityManager* integ) 
        : astNode(std::move(ast)), catalog(cat), storage(stor), integrity(integ) {}

    UpdateExecutor::~UpdateExecutor() {}

    bool UpdateExecutor::execute() {
        if (!astNode || !catalog || !storage || !integrity) return false;

        for (const auto& setValue : astNode->setValues) {
            // 使用工厂传进来的 integrity 实例，而不是自己重新去实例化
            if (!integrity->checkUpdate(astNode->tableName, setValue.field, setValue.newValue)) {
                return false;
            }
        }

        // TODO:
        
        return false;
    }

}
