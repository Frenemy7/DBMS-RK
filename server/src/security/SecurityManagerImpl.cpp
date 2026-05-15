#include "SecurityManagerImpl.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace Security {

    namespace {
        std::string roleToString(Role role) {
            return role == Role::ADMIN ? "ADMIN" : "USER";
        }

        Role roleFromString(const std::string& value) {
            return value == "ADMIN" ? Role::ADMIN : Role::USER;
        }
    }

    SecurityManagerImpl::SecurityManagerImpl(const std::string& userFile)
        : userFile_(userFile) {
        load();
        ensureDefaultUser();
        save();
    }

    bool SecurityManagerImpl::authenticate(const std::string& username, const std::string& password) {
        auto it = users_.find(username);
        return it != users_.end() && it->second.password == password;
    }

    bool SecurityManagerImpl::createUser(const std::string& username, const std::string& password) {
        if (username.empty() || users_.count(username) != 0) return false;
        users_[username] = UserInfo{password, Role::USER};
        save();
        return true;
    }

    bool SecurityManagerImpl::dropUser(const std::string& username) {
        if (username == "root") return false;
        if (users_.erase(username) == 0) return false;
        save();
        return true;
    }

    bool SecurityManagerImpl::grantRole(const std::string& username, Role role) {
        auto it = users_.find(username);
        if (it == users_.end()) return false;
        it->second.role = role;
        save();
        return true;
    }

    bool SecurityManagerImpl::revokeRole(const std::string& username) {
        auto it = users_.find(username);
        if (it == users_.end() || username == "root") return false;
        it->second.role = Role::USER;
        save();
        return true;
    }

    Role SecurityManagerImpl::getRole(const std::string& username) {
        auto it = users_.find(username);
        return it == users_.end() ? Role::USER : it->second.role;
    }

    void SecurityManagerImpl::load() {
        users_.clear();

        std::ifstream in(userFile_);
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            std::istringstream ss(line);
            std::string username;
            std::string password;
            std::string role;
            if (std::getline(ss, username, '|') &&
                std::getline(ss, password, '|') &&
                std::getline(ss, role)) {
                users_[username] = UserInfo{password, roleFromString(role)};
            }
        }
    }

    void SecurityManagerImpl::save() {
        std::filesystem::create_directories(std::filesystem::path(userFile_).parent_path());
        std::ofstream out(userFile_, std::ios::trunc);
        for (const auto& [username, info] : users_) {
            out << username << '|' << info.password << '|' << roleToString(info.role) << '\n';
        }
    }

    void SecurityManagerImpl::ensureDefaultUser() {
        if (users_.count("root") == 0) {
            users_["root"] = UserInfo{"123456", Role::ADMIN};
        }
    }

}
