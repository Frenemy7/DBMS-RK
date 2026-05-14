#include <iostream>
#include <string>
#include "src/storage/StorageEngineImpl.h"
#include "src/catalog/CatalogManagerImpl.h"
#include "src/parser/SQLParserImpl.h"
#include "src/execution/ExecutorFactory.h"
#include "src/integrity/IntegrityManagerImpl.h"
#include "src/maintenance/DatabaseMaintenanceImpl.h"
#include "src/security/SecurityManagerImpl.h"


using namespace std;

int main() {
    SetConsoleOutputCP(CP_UTF8);
    cout << "=========================================" << endl;
    cout << "        RuankoDBMS 命令行 MVP 版启动       " << endl;
    cout << "=========================================" << endl;

    // 1. 系统底层依赖注入（全生命周期唯一实例）
    Storage::StorageEngineImpl storageEngine;
    Catalog::CatalogManagerImpl catalogManager(&storageEngine);
    Integrity::IntegrityManagerImpl integrityManager(&catalogManager, &storageEngine);
    Maintenance::DatabaseMaintenanceImpl maintenance;
    Security::SecurityManagerImpl security;

    // 系统初始化：自动在硬盘创建 data 文件夹和 ruanko.db
    if (!catalogManager.initSystem()) {
        cerr << "致命错误：系统元数据初始化失败，进程退出。" << endl;
        return -1;
    }
    cout << "[系统] 底层存储引擎与数据字典加载完毕。" << endl;

    // 登录
    string currentUser;
    while (currentUser.empty()) {
        cout << "登录用户名: ";
        string user, pass;
        getline(cin, user);
        cout << "密码: ";
        getline(cin, pass);
        if (security.authenticate(user, pass)) {
            currentUser = user;
            auto role = security.getRole(user);
            cout << "欢迎, " << user << "! 角色: " << (role == Security::Role::ADMIN ? "ADMIN" : "USER") << endl;
        } else {
            cout << "认证失败，请重试。" << endl;
        }
    }

    // 2. 实例化解析器
    Parser::SQLParserImpl parser;

    // 3. 启动 REPL (Read-Eval-Print Loop)
    string sqlInput;
    while (true) {
        // 动态显示当前所在的数据库上下文
        string currentDb = catalogManager.getCurrentDatabase();
        if (currentDb.empty()) {
            cout << "\nRuankoDBMS > ";
        } else {
            cout << "\nRuankoDBMS [" << currentDb << "] > ";
        }

        // 获取用户输入
        getline(cin, sqlInput);

        // 拦截退出指令
        if (sqlInput == "exit" || sqlInput == "quit") {
            cout << "拜拜！系统安全关闭。" << endl;
            break;
        }
        if (sqlInput.empty()) continue;

        // 支持一行多语句，按分号分割
        std::string remaining = sqlInput;
        while (!remaining.empty()) {
            // 跳过前导空格
            size_t start = 0;
            while (start < remaining.size() && isspace(remaining[start])) start++;
            if (start >= remaining.size()) break;
            remaining = remaining.substr(start);

            // 查找分号位置
            size_t semi = remaining.find(';');
            if (semi == std::string::npos) break;
            std::string singleSql = remaining.substr(0, semi + 1);

            try {
                auto astTree = parser.parse(singleSql);

                // 权限拦截：非 ADMIN 不允许执行 DDL/用户管理
                if (astTree && security.getRole(currentUser) != Security::Role::ADMIN) {
                    auto t = astTree->getType();
                    if (t == Parser::SQLType::CREATE_TABLE || t == Parser::SQLType::DROP_TABLE ||
                        t == Parser::SQLType::ALTER_TABLE  || t == Parser::SQLType::CREATE_DATABASE ||
                        t == Parser::SQLType::DROP_DATABASE|| t == Parser::SQLType::CREATE_USER ||
                        t == Parser::SQLType::DROP_USER   || t == Parser::SQLType::GRANT_ROLE ||
                        t == Parser::SQLType::REVOKE_ROLE || t == Parser::SQLType::BACKUP_DATABASE ||
                        t == Parser::SQLType::RESTORE_DATABASE) {
                        cerr << "[权限] 拒绝: 当前用户 '" << currentUser << "' 无 DDL 权限（需 ADMIN 角色）。" << endl;
                        remaining = remaining.substr(semi + 1);
                        continue;
                    }
                }
                if (astTree) {
                    auto executor = Execution::ExecutorFactory::createExecutor(
                        std::move(astTree), &catalogManager, &storageEngine, &integrityManager, &maintenance, &security
                    );
                    if (executor) executor->execute();
                }
            } catch (const std::exception& e) {
                cerr << "[错误] " << e.what() << endl;
            }
            remaining = remaining.substr(semi + 1);
        }
    }

    return 0;
}
