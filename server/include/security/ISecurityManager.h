#ifndef ISECURITY_MANAGER_H
#define ISECURITY_MANAGER_H

#include <string>

namespace Security {

    enum class Role {
        USER = 0,
        ADMIN = 1
    };

    class ISecurityManager {
    public:
        virtual ~ISecurityManager() = default;

        virtual bool authenticate(const std::string& username, const std::string& password) = 0;
        virtual bool createUser(const std::string& username, const std::string& password) = 0;
        virtual bool dropUser(const std::string& username) = 0;
        virtual bool grantRole(const std::string& username, Role role) = 0;
        virtual bool revokeRole(const std::string& username) = 0;
        virtual Role getRole(const std::string& username) = 0;
    };

}

#endif // ISECURITY_MANAGER_H
