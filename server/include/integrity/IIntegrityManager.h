#ifndef IINTEGRITY_MANAGER_H
#define IINTEGRITY_MANAGER_H

#include <string>
#include <vector>

namespace Integrity {

    enum IntegrityType {
        PRIMARY_KEY = 1,
        FOREIGN_KEY = 2,
        UNIQUE = 3,
        NOT_NULL = 4
    };

    class IIntegrityManager {
    public:
        virtual bool checkInsert(
            const std::string& tableName,
            const std::vector<std::string>& columns,
            const std::vector<std::string>& values
        ) = 0;

        virtual bool checkUpdate(
            const std::string& tableName,
            const std::string& column,
            const std::string& value
        ) = 0;

        virtual bool checkDelete(
            const std::string& tableName,
            const std::string& pkValue
        ) = 0;

        virtual ~IIntegrityManager() = default;
    };

}

using Integrity::IIntegrityManager;

#endif // IINTEGRITY_MANAGER_H
