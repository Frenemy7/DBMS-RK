#ifndef SECURITY_MANAGER_IMPL_H
#define SECURITY_MANAGER_IMPL_H

#include "../../include/security/ISecurityManager.h"
#include <map>
#include <string>

namespace Security {

    class SecurityManagerImpl : public ISecurityManager {
    public:
        explicit SecurityManagerImpl(const std::string& userFile = "data/ruanko_users.usr");

        bool authenticate(const std::string& username, const std::string& password) override;
        bool createUser(const std::string& username, const std::string& password) override;
        bool dropUser(const std::string& username) override;
        bool grantRole(const std::string& username, Role role) override;
        bool revokeRole(const std::string& username) override;
        Role getRole(const std::string& username) override;

    private:
        struct UserInfo {
            std::string password;
            Role role = Role::USER;
        };

        std::string userFile_;
        std::map<std::string, UserInfo> users_;

        void load();
        void save();
        void ensureDefaultUser();
    };

}

#endif // SECURITY_MANAGER_IMPL_H
