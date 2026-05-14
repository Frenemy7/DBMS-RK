-- ============================================================
-- 新功能测试：IDENTITY / CHECK / DEFAULT / 安全性 / 备份还原
-- 使用 CREATE DATABASE testnew; USE DATABASE testnew;
-- ============================================================

-- ============================================================
-- 十一、IDENTITY 自增
-- ============================================================

-- 11.1 基本自增
CREATE TABLE t_id (pk INT PRIMARY KEY IDENTITY, val VARCHAR(10));
INSERT INTO t_id (val) VALUES ('A');
INSERT INTO t_id (val) VALUES ('B');
INSERT INTO t_id (val) VALUES ('C');
SELECT * FROM t_id;
-- 预期: 1|A, 2|B, 3|C

-- 11.2 手动指定 ID 跳过自增
INSERT INTO t_id (pk, val) VALUES (100, 'manual');
SELECT * FROM t_id;
-- 预期: 100|manual 出现，不冲突

-- 11.3 自增从最大+1 继续
INSERT INTO t_id (val) VALUES ('after100');
SELECT * FROM t_id;
-- 预期: pk=101 的新行




-- ============================================================
-- 十二、CHECK 约束
-- ============================================================

-- 12.1 单列 CHECK
CREATE TABLE t_chk1 (id INT PRIMARY KEY, age INT CHECK(age > 0));
INSERT INTO t_chk1 VALUES (1, 25);
-- 预期: OK
INSERT INTO t_chk1 VALUES (2, -5);
-- 预期: Error CHECK 约束违反

-- 12.2 多条件 CHECK（AND/OR）
CREATE TABLE t_chk2 (id INT PRIMARY KEY, score INT CHECK(score >= 0 AND score <= 100));
INSERT INTO t_chk2 VALUES (1, 50);
-- 预期: OK
INSERT INTO t_chk2 VALUES (2, 150);
-- 预期: Error CHECK 约束违反
INSERT INTO t_chk2 VALUES (3, -10);
-- 预期: Error CHECK 约束违反

-- 12.3 UPDATE 受 CHECK 约束
UPDATE t_chk2 SET score = 200 WHERE id = 1;
-- 预期: Error CHECK 约束违反

-- 12.4 CHECK 与其他约束共存
CREATE TABLE t_chk3 (id INT PRIMARY KEY, name VARCHAR(10) NOT NULL DEFAULT 'N/A', qty INT CHECK(qty > 0));
INSERT INTO t_chk3 (id, qty) VALUES (1, 10);
SELECT * FROM t_chk3;
-- 预期: 1|N/A|10（DEFAULT 生效 + CHECK 通过）


-- ============================================================
-- 十三、DEFAULT 约束
-- ============================================================

-- 13.1 基本默认值
CREATE TABLE t_def (id INT PRIMARY KEY, status VARCHAR(10) DEFAULT 'active', cnt INT DEFAULT 0);
INSERT INTO t_def (id) VALUES (1);
INSERT INTO t_def (id, status) VALUES (2, 'inactive');
INSERT INTO t_def (id, cnt) VALUES (3, 99);
SELECT * FROM t_def;
-- 预期: 1|active|0, 2|inactive|0, 3|active|99

-- 13.2 INSERT SELECT 也适用 DEFAULT
INSERT INTO t_def (id) SELECT 4;
SELECT * FROM t_def WHERE id = 4;
-- 预期: 4|active|0


-- ============================================================
-- 十四、安全性管理
-- ============================================================

-- 14.1 创建用户
CREATE USER bob IDENTIFIED BY 'bob123';
-- 预期: Query OK

-- 14.2 重复创建
CREATE USER bob IDENTIFIED BY 'another';
-- 预期: Error 创建用户失败

-- 14.3 授权
GRANT ADMIN bob;
-- 预期: Query OK

-- 14.4 撤销权限
REVOKE bob;
-- 预期: Query OK（角色降为 USER）

-- 14.5 删除用户
DROP USER bob;
-- 预期: Query OK

-- 14.6 边界：删除不存在的用户
DROP USER nobody;
-- 预期: Error


-- ============================================================
-- 十五、备份还原（换到 root 目录可写的路径）
-- ============================================================

-- 先切到有空闲磁盘的地方
BACKUP DATABASE testnew TO 'data/backup_test';
-- 预期: Query OK

-- 验证备份存在后删库还原
DROP DATABASE testnew;
RESTORE DATABASE testnew FROM 'data/backup_test/testnew';
USE DATABASE testnew;
SELECT * FROM t_def;
-- 预期: 数据完整恢复
