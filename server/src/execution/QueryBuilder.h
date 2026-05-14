#ifndef QUERY_BUILDER_H
#define QUERY_BUILDER_H

#include "../../include/execution/IOperator.h"
#include "../../include/parser/ASTNode.h"
#include <memory>
#include <vector>
#include <string>

namespace Catalog { class ICatalogManager; }
namespace Storage { class IStorageEngine; }
namespace Index { class IIndexManager; }
namespace Meta { struct FieldBlock; }

namespace Execution {

std::unique_ptr<IOperator> buildTableSource(Parser::ASTNode* source,
                                             Catalog::ICatalogManager* catalog,
                                             Storage::IStorageEngine* storage,
                                             Index::IIndexManager* index = nullptr,
                                             Parser::ASTNode* where = nullptr);

// outerRow/outerSchema/outerAlias: 关联子查询的外部上下文（可为 nullptr）
std::vector<std::vector<std::string>> runSubquery(Parser::ASTNode* subqueryRoot,
                                                   Catalog::ICatalogManager* catalog,
                                                   Storage::IStorageEngine* storage,
                                                   const std::vector<std::string>* outerRow = nullptr,
                                                   const std::vector<Meta::FieldBlock>* outerSchema = nullptr,
                                                   const std::string* outerAlias = nullptr);

} // namespace Execution
#endif
