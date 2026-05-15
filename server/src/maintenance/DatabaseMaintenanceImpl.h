#ifndef DATABASE_MAINTENANCE_IMPL_H
#define DATABASE_MAINTENANCE_IMPL_H

#include "../../include/maintenance/IDatabaseMaintenance.h"

namespace Maintenance {

    class DatabaseMaintenanceImpl : public IDatabaseMaintenance {
    public:
        bool backupDatabase(const std::string& dbName, const std::string& path) override;
        bool restoreDatabase(const std::string& dbName, const std::string& path) override;
        bool backupExists(const std::string& path) override;
    };

}

#endif // DATABASE_MAINTENANCE_IMPL_H
