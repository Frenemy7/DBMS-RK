#include <iostream>
#include <string>
#include "src/storage/StorageEngineImpl.h"
#include "src/catalog/CatalogManagerImpl.h"
#include "src/parser/SQLParserImpl.h"
#include "src/execution/ExecutorFactory.h"
#include "src/integrity/IntegrityManagerImpl.h"


using namespace std;

int main() {
    cout << "=========================================" << endl;
    cout << "        RuankoDBMS 命令行 MVP 版启动       " << endl;
    cout << "=========================================" << endl;

    // 1. 系统底层依赖注入（全生命周期唯一实例）
    Storage::StorageEngineImpl storageEngine;
    Catalog::CatalogManagerImpl catalogManager(&storageEngine);
    Integrity::IntegrityManagerImpl integrityManager(&catalogManager, &storageEngine);

    // 系统初始化：自动在硬盘创建 data 文件夹和 ruanko.db
    if (!catalogManager.initSystem()) {
        cerr << "致命错误：系统元数据初始化失败，进程退出。" << endl;
        return -1;
    }
    cout << "[系统] 底层存储引擎与数据字典加载完毕。" << endl;

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

        try {
            // 第一关：大脑翻译（文本 -> AST 树）
            auto astTree = parser.parse(sqlInput);
            if (!astTree) continue;

            // 第二关：中枢分配（AST 树 -> 执行器）
            auto executor = Execution::ExecutorFactory::createExecutor(
                std::move(astTree), &catalogManager, &storageEngine, &integrityManager
            );

            // 第三关：手脚落盘（执行底层的读写逻辑）
            if (executor) {
                executor->execute();
            }

        } catch (const std::exception& e) {
            // 捕获 Parser 的语法错误或 Executor 的运行时错误，并友善地打印给用户
            cerr << "[错误] " << e.what() << endl;
        }
    }

    return 0;
}