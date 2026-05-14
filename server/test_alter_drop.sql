-- ============================================================
--  ALTER TABLE / DROP TABLE 功能测试（需求 3.3.2 / 3.3.3 / 3.4）
--  使用前请确保已: CREATE DATABASE testdb; USE DATABASE testdb;
-- ============================================================

-- ============================================================
-- 准备：建测试用表
-- ============================================================

-- T1: 基础表
CREATE TABLE test_t1 (id INT PRIMARY KEY, name VARCHAR(20), score INT);
-- T2: 多类型表
CREATE TABLE test_t2 (pk INT PRIMARY KEY, label VARCHAR(10), amount DOUBLE, created DATETIME);

INSERT INTO test_t1 (id, name, score) VALUES (1, 'Alice', 85);
INSERT INTO test_t1 (id, name, score) VALUES (2, 'Bob', 92);
INSERT INTO test_t1 (id, name, score) VALUES (3, 'Cathy', 78);

INSERT INTO test_t2 (pk, label, amount, created) VALUES (1, 'Sale', 99.5, '2026-01-15');
INSERT INTO test_t2 (pk, label, amount, created) VALUES (2, 'Refund', 20.0, '2026-03-20');


-- ============================================================
-- 一、需求 3.4.1 添加字段（ALTER TABLE ADD COLUMN）
-- ============================================================

-- 1.1 添加整型列
ALTER TABLE test_t1 ADD COLUMN age INT;
SELECT * FROM test_t1;
-- 预期: 3行，新增 age 列，值为空

-- 1.2 添加 VARCHAR 列
ALTER TABLE test_t1 ADD COLUMN email VARCHAR(32);
SELECT * FROM test_t1;
-- 预期: 新列 email

-- 1.3 添加 DATETIME 列
ALTER TABLE test_t2 ADD COLUMN updated DATETIME;
SELECT * FROM test_t2;

-- 1.4 添加 DOUBLE 列
ALTER TABLE test_t2 ADD COLUMN tax_rate DOUBLE;
SELECT * FROM test_t2;

-- 1.5 边界：重复添加同名列
ALTER TABLE test_t1 ADD COLUMN age INT;
-- 预期: Error 提示列已存在


-- ============================================================
-- 二、需求 3.4.2 修改字段（ALTER TABLE MODIFY COLUMN）
-- ============================================================

-- 2.1 修改列类型: INT → VARCHAR
ALTER TABLE test_t1 MODIFY COLUMN age VARCHAR(8);
SELECT * FROM test_t1;

-- 2.2 修改列类型参数: VARCHAR(20) → VARCHAR(50)
ALTER TABLE test_t1 MODIFY COLUMN name VARCHAR(50);
SELECT * FROM test_t1;

-- 2.3 边界：修改不存在的列
ALTER TABLE test_t1 MODIFY COLUMN not_exist INT;
-- 预期: Error 提示列不存在


-- ============================================================
-- 三、需求 3.4.3 删除字段（ALTER TABLE DROP COLUMN）
-- ============================================================

-- 3.1 删除列
ALTER TABLE test_t1 DROP COLUMN email;
SELECT * FROM test_t1;
-- 预期: email 列消失，剩余列正常显示

-- 3.2 连续删除多列
ALTER TABLE test_t1 DROP COLUMN age;
ALTER TABLE test_t2 DROP COLUMN updated;
SELECT * FROM test_t1;
SELECT * FROM test_t2;

-- 3.3 边界：删除不存在的列
ALTER TABLE test_t1 DROP COLUMN not_exist;
-- 预期: Error 提示列不存在


-- ============================================================
-- 四、需求 3.3.3 删除表（DROP TABLE）
-- ============================================================

-- 4.1 创建临时表 → 删表
CREATE TABLE temp_drop (x INT, y VARCHAR(10));
INSERT INTO temp_drop (x, y) VALUES (1, 'hello');
SELECT * FROM temp_drop;
-- 预期: 1 行

DROP TABLE temp_drop;
-- 预期: Query OK. 表已删除

-- 4.2 验证删除后无法查询
SELECT * FROM temp_drop;
-- 预期: Error 提示表不存在

-- 4.3 重建同名表（验证删表后文件清理完毕）
CREATE TABLE temp_drop (a INT PRIMARY KEY, b DOUBLE);
INSERT INTO temp_drop (a, b) VALUES (100, 3.14);
SELECT * FROM temp_drop;
-- 预期: 1 行，无历史数据残留

-- 4.4 边界：删除不存在的表
DROP TABLE not_a_table;
-- 预期: Error 提示表不存在


-- ============================================================
-- 五、组合测试
-- ============================================================

-- 5.1 ADD + MODIFY + DROP 连续操作
CREATE TABLE combo_test (id INT PRIMARY KEY);
ALTER TABLE combo_test ADD COLUMN val1 INT;
ALTER TABLE combo_test ADD COLUMN val2 VARCHAR(10);
ALTER TABLE combo_test MODIFY COLUMN val1 DOUBLE;
ALTER TABLE combo_test DROP COLUMN val2;
SELECT * FROM combo_test;
-- 预期: id + val1(DOUBLE) 两列，无数据

-- 5.2 清理
DROP TABLE combo_test;
-- 预期: 成功


-- ============================================================
-- 六、对比需求报告功能点
-- ============================================================
-- 3.3.2 修改表      ✓ ALTER TABLE ... ADD/MODIFY/DROP COLUMN
-- 3.3.3 删除表      ✓ DROP TABLE
-- 3.4.1 添加字段    ✓ ALTER TABLE ADD COLUMN
-- 3.4.2 修改字段    ✓ ALTER TABLE MODIFY COLUMN
-- 3.4.3 删除字段    ✓ ALTER TABLE DROP COLUMN
--
-- 已知限制（需求中但未实现）：
-- - MODIFY/DROP COLUMN 时不更新已有记录的值
-- - DROP COLUMN 时不检查/清理关联索引
-- - DROP TABLE 时不做 RESTRICT/CASCADE 判断
