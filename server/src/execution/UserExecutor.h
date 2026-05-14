#ifndef USER_EXECUTOR_H
#define USER_EXECUTOR_H

#include "../../include/execution/IExecutor.h"
#include "../../include/security/ISecurityManager.h"
#include <string>

namespace Execution {

    // 统一处理 CREATE USER / DROP USER / GRANT / REVOKE
    class UserExecutor : public IExecutor {
    public:
        enum class Op { CREATE_USER, DROP_USER, GRANT_ROLE, REVOKE_ROLE };
    private:
        Op op_;
        std::string username_;
        std::string password_;
        std::string role_;
        Security::ISecurityManager* sec_;

    public:
        UserExecutor(Op op, const std::string& user, const std::string& pwd,
                    const std::string& role, Security::ISecurityManager* sec);
        ~UserExecutor() override = default;
        bool execute() override;
    };

}
#endif
