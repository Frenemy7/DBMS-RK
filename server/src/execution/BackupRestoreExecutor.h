#ifndef BACKUP_RESTORE_EXECUTOR_H
#define BACKUP_RESTORE_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/maintenance/IDatabaseMaintenance.h"
#include "../../include/catalog/ICatalogManager.h"
#include <string>

namespace Execution {

    class BackupRestoreExecutor : public IExecutor {
    public:
        enum class Mode { BACKUP, RESTORE };
    private:
        Mode mode_;
        std::string dbName_;
        std::string path_;
        Catalog::ICatalogManager* catalog_;
        Maintenance::IDatabaseMaintenance* maintenance_;

    public:
        BackupRestoreExecutor(Mode mode, const std::string& dbName,
                             const std::string& path,
                             Catalog::ICatalogManager* catalog,
                             Maintenance::IDatabaseMaintenance* maintenance);
        ~BackupRestoreExecutor() override = default;
        bool execute() override;
    };

}
#endif
