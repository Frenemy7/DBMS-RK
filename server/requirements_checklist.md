# 需求完成度检查清单

根据报告第22章"4 Requirements Classification 需求分级"逐项对照。

| Requirement ID | Requirement Name | Classification | 状态 | 说明 |
|---|---|---|---|---|
| 3.2.1 | 创建数据库 | A | ✅ | `CREATE DATABASE name` |
| 3.2.2 | 删除数据库 | C | ✅ | `DROP DATABASE name` |
| 3.3.1 | 创建表 | A | ✅ | `CREATE TABLE name (cols)` |
| 3.3.2 | 修改表 | C | ✅ | `ALTER TABLE ADD/MODIFY/DROP COLUMN` |
| 3.3.3 | 删除表 | C | ✅ | `DROP TABLE name` |
| 3.4.1 | 添加字段 | A | ✅ | `ALTER TABLE ADD COLUMN` |
| 3.4.2 | 修改字段 | C | ✅ | `ALTER TABLE MODIFY COLUMN` |
| 3.4.3 | 删除字段 | B | ✅ | `ALTER TABLE DROP COLUMN` |
| 3.5.1 | 插入记录 | A | ✅ | `INSERT VALUES` + `INSERT SELECT` |
| 3.5.2 | 更新记录 | B | ✅ | `UPDATE SET WHERE` |
| 3.5.3 | 查询记录 | A | ✅ | `SELECT`（含 JOIN/子查询/GROUP BY/HAVING/ORDER BY/聚合/算术） |
| 3.5.4 | 删除记录 | C | ✅ | `DELETE FROM WHERE` |
| 3.6 | 索引管理 | C | ❌ | 仅 CatalogManager 接口层，Parser+Executor 未实现 |
| 3.7 | 客户端管理 | C | ❌ | 未实现 |
| 3.8 | 事务管理 | C | ❌ | 未实现 |
| 3.9 | 完整性管理 | C | ✅ | PK/NOT NULL/UNIQUE/FK/DEFAULT/CHECK/IDENTITY 全部实现 |
| 3.10 | 数据库维护 | C | ✅ | `BACKUP/RESTORE DATABASE` |
| 3.11 | 安全性管理 | C | ✅ | `CREATE/DROP USER`、`GRANT/REVOKE`、密码哈希、角色管理 |

### 统计

| Classification | 完成/总数 |
|---|---|
| A（必须的） | **5/5** |
| B（重要的） | **2/2** |
| C（最好有的） | **8/11** |
| 总计 | **15/18** |
