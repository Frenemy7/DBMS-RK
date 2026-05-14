#include "UserExecutor.h"
#include <iostream>

namespace Execution {

    UserExecutor::UserExecutor(Op op, const std::string& user, const std::string& pwd,
                               const std::string& role, Security::ISecurityManager* sec)
        : op_(op), username_(user), password_(pwd), role_(role), sec_(sec) {}

    bool UserExecutor::execute() {
        if (!sec_) return false;

        switch (op_) {
            case Op::CREATE_USER:
                if (sec_->createUser(username_, password_))
                    std::cout << "Query OK. 用户 '" << username_ << "' 已创建。" << std::endl;
                else { std::cerr << "Error: 创建用户失败。" << std::endl; return false; }
                break;
            case Op::DROP_USER:
                if (sec_->dropUser(username_))
                    std::cout << "Query OK. 用户 '" << username_ << "' 已删除。" << std::endl;
                else { std::cerr << "Error: 删除用户失败。" << std::endl; return false; }
                break;
            case Op::GRANT_ROLE: {
                auto r = (role_ == "ADMIN") ? Security::Role::ADMIN : Security::Role::USER;
                if (sec_->grantRole(username_, r))
                    std::cout << "Query OK. 已授予 " << role_ << " 给 '" << username_ << "'。" << std::endl;
                else { std::cerr << "Error: 授权失败。" << std::endl; return false; }
                break;
            }
            case Op::REVOKE_ROLE:
                if (sec_->revokeRole(username_))
                    std::cout << "Query OK. 已撤销 '" << username_ << "' 的权限。" << std::endl;
                else { std::cerr << "Error: 撤销权限失败。" << std::endl; return false; }
                break;
        }
        return true;
    }

}
