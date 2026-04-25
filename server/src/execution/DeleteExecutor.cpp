#include "DeleteExecutor.h"
#include "operators/SeqScanOperator.h"
#include "operators/FilterOperator.h"
#include "../../include/common/Constants.h"
#include <iostream>
#include <set>
#include <cstring>
#include <algorithm>
#include <fstream>

namespace Execution {

    DeleteExecutor::DeleteExecutor(std::unique_ptr<Parser::DeleteASTNode> ast,
                                   Catalog::ICatalogManager* catalog,
                                   Storage::IStorageEngine* storage,
                                   Integrity::IIntegrityManager* integrity)
        : astNode_(std::move(ast)), catalogManager_(catalog), 
          storageEngine_(storage), integrityManager_(integrity) {}

    bool DeleteExecutor::execute() {
        if (!astNode_) return false;
        std::string tableName = astNode_->tableName;

        if (!catalogManager_->hasTable(tableName)) {
            std::cerr << "Error: 表 '" << tableName << "' 不存在。" << std::endl;
            return false;
        }

        // 1. 获取表的物理元数据，并找出主键列的名字和物理偏移量
        auto fields = catalogManager_->getFields(tableName);
        std::sort(fields.begin(), fields.end(), [](const Meta::FieldBlock& a, const Meta::FieldBlock& b) {
            return a.order < b.order;
        });

        std::string pkFieldName = "";
        auto integrities = catalogManager_->getIntegrities(tableName);
        for (const auto& ig : integrities) {
            if (ig.type == Integrity::PRIMARY_KEY) { // 对齐你组员的枚举
                pkFieldName = ig.field;
                break;
            }
        }

        if (pkFieldName.empty()) {
            std::cerr << "Error: 无法执行 DELETE。表 '" << tableName << "' 没有主键，无法确保删除安全。" << std::endl;
            return false;
        }

        // ========================================================
        // 阶段一：逻辑查找（复用流水线，找出所有需要被枪毙的主键值）
        // ========================================================
        std::set<std::string> pksToDelete;

        std::unique_ptr<IOperator> rootOperator = std::make_unique<SeqScanOperator>(tableName, catalogManager_, storageEngine_);
        if (astNode_->whereExpressionTree != nullptr) {
            rootOperator = std::make_unique<FilterOperator>(std::move(rootOperator), astNode_->whereExpressionTree.get());
        }

        if (!rootOperator->init()) return false;
        
        auto schema = rootOperator->getOutputSchema();
        int pkIndex = -1;
        for (size_t i = 0; i < schema.size(); ++i) {
            if (std::string(schema[i].name) == pkFieldName) pkIndex = i;
        }

        std::vector<std::string> row;
        while (rootOperator->next(row)) {
            std::string pkValue = row[pkIndex];

            // 呼叫组员的安保系统，检查是否有外键约束拦着不让删
            if (!integrityManager_->checkDelete(tableName, pkValue)) {
                std::cerr << "Error: 违反外键约束！记录 (PK=" << pkValue << ") 正在被其他表引用，拒绝删除。" << std::endl;
                return false; 
            }
            pksToDelete.insert(pkValue);
        }

        if (pksToDelete.empty()) {
            std::cout << "Query OK. 0 rows affected." << std::endl;
            return true;
        }

        // ========================================================
        // 阶段二：物理重写（将不需要删除的记录重新覆盖进硬盘）
        // ========================================================
        Meta::TableBlock tableMeta = catalogManager_->getTableMeta(tableName);
        std::string trdPath = "data/" + catalogManager_->getCurrentDatabase() + "/" + std::string(tableMeta.trd);
        long fileSize = storageEngine_->getFileSize(trdPath);

        // 计算单行记录物理大小，并找到主键在这一行二进制数据里的物理偏移量
        int recordSize = 0;
        int pkPhysicalOffset = 0;
        int currentOffset = 0;
        int pkType = 0;

        for (const auto& field : fields) {
            if (std::string(field.name) == pkFieldName) {
                pkPhysicalOffset = currentOffset;
                pkType = field.type;
            }

            if (field.type == static_cast<int>(Common::DataType::INTEGER)) {
                currentOffset += 4;
            } else if (field.type == static_cast<int>(Common::DataType::VARCHAR)) {
                int size = (field.param > 0) ? field.param : Common::MAX_PATH_LEN;
                currentOffset += ((size + 3) / 4) * 4;
            }
        }
        recordSize = currentOffset;

        // 把旧文件全读出来
        char* oldBuffer = new char[fileSize];
        storageEngine_->readRaw(trdPath, 0, fileSize, oldBuffer);

        // 准备一个新容器装活下来的数据
        char* newBuffer = new char[fileSize]; 
        int newBufferCursor = 0;
        int deletedCount = 0;

        int recordCount = fileSize / recordSize;
        for (int i = 0; i < recordCount; ++i) {
            char* recordPtr = oldBuffer + (i * recordSize);
            char* pkPtr = recordPtr + pkPhysicalOffset;

            // 把这行的物理主键提出来，转成字符串去 set 里查
            std::string currentPkStr;
            if (pkType == static_cast<int>(Common::DataType::INTEGER)) {
                int val;
                std::memcpy(&val, pkPtr, sizeof(int));
                currentPkStr = std::to_string(val);
            } else if (pkType == static_cast<int>(Common::DataType::VARCHAR)) {
                currentPkStr = std::string(pkPtr);
            }

            // 如果主键在死亡名单里，跳过不写入；否则拷贝到新 buffer
            if (pksToDelete.find(currentPkStr) != pksToDelete.end()) {
                deletedCount++;
            } else {
                std::memcpy(newBuffer + newBufferCursor, recordPtr, recordSize);
                newBufferCursor += recordSize;
            }
        }

        // 把洗干净的数据重新拍进硬盘（覆盖写）
        // 这里的 writeRaw 如果你们底层没有覆盖写接口，可以用 C++ 原生 fstream 处理
        std::ofstream outfile(trdPath, std::ios::binary | std::ios::trunc);
        outfile.write(newBuffer, newBufferCursor);
        outfile.close();

        // 扫地出门，更新数据字典
        tableMeta.record_num -= deletedCount;
        catalogManager_->updateTableMeta(tableName, tableMeta);

        delete[] oldBuffer;
        delete[] newBuffer;

        std::cout << "Query OK. " << deletedCount << " rows affected." << std::endl;
        return true;
    }
}