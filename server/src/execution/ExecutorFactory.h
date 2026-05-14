#ifndef EXECUTOR_FACTORY_H
#define EXECUTOR_FACTORY_H

#include "../../include/parser/ASTNode.h"
#include "../../include/execution/IExecutor.h"
#include "../../include/catalog/ICatalogManager.h"
#include "../../include/storage/IStorageEngine.h"
#include "../../include/integrity/IIntegrityManager.h"
#include <memory>

namespace Index { class IIndexManager; }
namespace Maintenance { class IDatabaseMaintenance; }
namespace Security { class ISecurityManager; }

namespace Execution {

    class ExecutorFactory {
    public:
        static std::unique_ptr<IExecutor> createExecutor(
            std::unique_ptr<Parser::ASTNode> ast,
            Catalog::ICatalogManager* catalog,
            Storage::IStorageEngine* storage,
            Integrity::IIntegrityManager* integrity,
            Maintenance::IDatabaseMaintenance* maintenance = nullptr,
            Security::ISecurityManager* security = nullptr,
            Index::IIndexManager* index = nullptr
        );
    };

} // namespace Execution

#endif // EXECUTOR_FACTORY_H
