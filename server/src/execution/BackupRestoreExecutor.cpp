#include "BackupRestoreExecutor.h"
#include "../../include/maintenance/IDatabaseMaintenance.h"
#include <iostream>

namespace Execution {

    BackupRestoreExecutor::BackupRestoreExecutor(Mode mode, const std::string& dbName,
                                                   const std::string& path,
                                                   Catalog::ICatalogManager* catalog,
                                                   Maintenance::IDatabaseMaintenance* maintenance)
        : mode_(mode), dbName_(dbName), path_(path), catalog_(catalog), maintenance_(maintenance) {}

    bool BackupRestoreExecutor::execute() {
        if (!maintenance_) return false;

        if (mode_ == Mode::BACKUP) {
            if (!catalog_->hasDatabase(dbName_)) {
                std::cerr << "Error: 数据库 '" << dbName_ << "' 不存在。" << std::endl;
                return false;
            }
            if (!maintenance_->backupDatabase(dbName_, path_)) {
                std::cerr << "Error: 备份数据库 '" << dbName_ << "' 失败。" << std::endl;
                return false;
            }
            std::cout << "Query OK. 数据库 '" << dbName_ << "' 已备份到 '" << path_ << "'。" << std::endl;
        } else {
            if (!maintenance_->backupExists(path_)) {
                std::cerr << "Error: 备份 '" << path_ << "' 不存在。" << std::endl;
                return false;
            }
            if (!maintenance_->restoreDatabase(dbName_, path_)) {
                std::cerr << "Error: 还原数据库 '" << dbName_ << "' 失败。" << std::endl;
                return false;
            }
            std::cout << "Query OK. 数据库 '" << dbName_ << "' 已从 '" << path_ << "' 还原。" << std::endl;
        }
        return true;
    }

}
