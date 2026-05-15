#ifndef IDATABASE_MAINTENANCE_H
#define IDATABASE_MAINTENANCE_H

#include <string>

namespace Maintenance {

    class IDatabaseMaintenance {
    public:
        virtual ~IDatabaseMaintenance() = default;

        virtual bool backupDatabase(const std::string& dbName, const std::string& path) = 0;
        virtual bool restoreDatabase(const std::string& dbName, const std::string& path) = 0;
        virtual bool backupExists(const std::string& path) = 0;
    };

}

#endif // IDATABASE_MAINTENANCE_H
