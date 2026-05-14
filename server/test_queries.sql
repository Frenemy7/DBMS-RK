-- ============================================================
--  SELECT 完整功能测试集
--  使用前请先执行 create_tables.sql 和 insert_data.sql 建表并插入数据
-- ============================================================

-- ============================================================
-- 一、基础查询（已有功能，回归验证）
-- ============================================================

-- 1.1 全表查询
SELECT * FROM student;
-- 预期: 7行，每行6列（sno, sname, ssex, sbirthday, classno, totalcredit）

-- 1.2 指定列
SELECT sno, sname FROM student;
-- 预期: 7行，每行2列

-- 1.3 简单 WHERE 等值过滤
SELECT * FROM student WHERE ssex = 'M';
-- 预期: 只返回男同学（Tom, Jack, Mike, John）


-- ============================================================
-- 二、点号引用（table.column） + 表别名  【新功能】
-- ============================================================

-- 2.1 基本点号引用
SELECT student.sno, student.sname FROM student;
-- 测试: 解析器能否识别 "表名.列名" 语法

-- 2.2 表别名 + 点号引用
SELECT s.sno, s.sname FROM student s;
-- 测试: 解析器能否同时处理 AS 别名和点号

-- 2.3 显式 AS 别名
SELECT st.sno, st.sname FROM student AS st;
-- 测试: 显式 AS 关键字


-- ============================================================
-- 三、WHERE 逻辑运算 【新功能】
-- ============================================================

-- 3.1 AND 多条件
SELECT * FROM student WHERE ssex = 'M' AND classno = 'Rj0801';
-- 测试: AND 运算符

-- 3.2 OR 条件
SELECT * FROM student WHERE classno = 'Rj0801' OR classno = 'Rj0802';
-- 测试: OR 运算符

-- 3.3 AND + OR 优先级
SELECT * FROM student WHERE ssex = 'M' AND (classno = 'Rj0801' OR classno = 'Rj0802');
-- 测试: 括号优先级 + AND/OR 组合

-- 3.4 LIKE 模糊匹配
SELECT * FROM student WHERE sname LIKE 'J%';
-- 预期: Jack, John
-- 测试: LIKE 前缀匹配

-- 3.5 IS NULL / IS NOT NULL
SELECT * FROM sc WHERE grade IS NOT NULL;
-- 预期: 所有有成绩的选课记录（6行）
-- 测试: IS NOT NULL

SELECT * FROM sc WHERE grade IS NULL;
-- 预期: 0行（因为我们只插入了有成绩的记录）
-- 测试: IS NULL

-- 3.6 BETWEEN
SELECT * FROM sc WHERE grade BETWEEN 70 AND 90;
-- 预期: grade 在 70~90 之间的记录
-- 测试: BETWEEN ... AND ...

-- 3.7 IN 列表
SELECT * FROM student WHERE classno IN ('Rj0801', 'Rj0803', 'Rj0805');
-- 测试: IN (值列表)


-- ============================================================
-- 四、JOIN 连接查询 【新功能】
-- ============================================================

-- 4.1 单表取别名 (非 JOIN，但验证基础)
SELECT s.sname FROM student s;
-- 测试: 普通别名

-- 4.2 INNER JOIN
SELECT s.sname, sc.grade FROM student s JOIN sc ON s.sno = sc.sno;
-- 预期: 每个选了课的学生信息+成绩
-- 测试: INNER JOIN + ON 条件

-- 4.3 LEFT JOIN (保留左表全行)
SELECT s.sname, sc.grade FROM student s LEFT JOIN sc ON s.sno = sc.sno;
-- 预期: 所有7个学生，没选课的grade显示为空
-- 测试: LEFT JOIN 保留左表未匹配行

-- 4.4 LEFT JOIN 带 AND 条件
SELECT s.sname, sc.grade FROM student s LEFT JOIN sc ON s.sno = sc.sno AND sc.grade >= 80;
-- 预期: 所有学生，但只有 >=80 分的选课才连接上
-- 测试: JOIN ON 中 AND 条件

-- 4.5 逗号隐式 CROSS JOIN
SELECT s.sname, c.cname FROM student s, course c WHERE s.sno = '08300010';
-- 预期: Tom × 4门课 = 4行
-- 测试: 逗号分隔的多表查询（隐式 CROSS JOIN + WHERE 过滤）

-- 4.6 链式 JOIN（两表 → 三表）
SELECT s.sname, c.cname, sc.grade FROM student s JOIN sc ON s.sno = sc.sno JOIN course c ON sc.cno = c.cno;
-- 测试: 链式连接，student → sc → course

-- 4.7 LEFT JOIN 链式
SELECT s.sname, c.cname, sc.grade
FROM student s
LEFT JOIN sc ON s.sno = sc.sno
LEFT JOIN course c ON sc.cno = c.cno;
-- 预期: 所有7个学生，选课和课程信息有则显示
-- 测试: 链式 LEFT JOIN


-- ============================================================
-- 五、聚合函数 + GROUP BY 【新功能】
-- ============================================================

-- 5.1 COUNT(*)
SELECT COUNT(*) FROM student;
-- 预期: 7
-- 测试: 基本 COUNT

-- 5.2 COUNT 带 GROUP BY
SELECT ssex, COUNT(*) FROM student GROUP BY ssex;
-- 预期: M=4, F=3
-- 测试: GROUP BY + COUNT 聚合

-- 5.3 SUM
SELECT ssex, SUM(totalcredit) FROM student GROUP BY ssex;
-- 预期: 当前 totalcredit 全是 0，所以 SUM=0
-- 测试: SUM 聚合

-- 5.4 AVG
SELECT cno, AVG(grade) FROM sc GROUP BY cno;
-- 预期: 800003 平均分 (88+91+78+95)/4=88，810011 平均分 (67+58)/2=62.5
-- 测试: AVG 聚合

-- 5.5 MAX / MIN
SELECT MAX(grade), MIN(grade) FROM sc;
-- 预期: MAX=95, MIN=58
-- 测试: MAX 和 MIN 聚合

-- 5.6 多聚合函数
SELECT COUNT(*), AVG(grade), MAX(grade), MIN(grade) FROM sc;
-- 测试: 一条 SELECT 中包含多个聚合函数

-- 5.7 GROUP BY 多列
SELECT ssex, classno, COUNT(*) FROM student GROUP BY ssex, classno;
-- 测试: 多列分组


-- ============================================================
-- 六、ORDER BY 排序 【新功能】
-- ============================================================

-- 6.1 默认升序
SELECT sno, sname, sbirthday FROM student ORDER BY sbirthday;
-- 预期: 按出生日期从早到晚排列

-- 6.2 显式 ASC
SELECT sno, sname FROM student ORDER BY sname ASC;
-- 测试: 显式 ASC

-- 6.3 DESC 降序
SELECT cno, grade FROM sc ORDER BY grade DESC;
-- 预期: 成绩从高到低排列 (95, 91, 88, 78, 67, 58)
-- 测试: DESC 降序

-- 6.4 多列排序
SELECT * FROM student ORDER BY classno ASC, sname DESC;
-- 测试: 多列排序


-- ============================================================
-- 七、组合查询 【新功能】
-- ============================================================

-- 7.1 JOIN + WHERE + ORDER BY
SELECT s.sname, sc.grade FROM student s JOIN sc ON s.sno = sc.sno
WHERE sc.grade >= 80 ORDER BY sc.grade DESC;
-- 测试: JOIN + WHERE 过滤 + ORDER BY 排序，三层算子串行

-- 7.2 GROUP BY + HAVING（通过 WHERE 在分组前过滤模拟）
SELECT classno, COUNT(*) FROM student GROUP BY classno;
-- 预期: 按班级统计人数
-- 测试: GROUP BY + COUNT

-- 7.3 JOIN + GROUP BY + ORDER BY
SELECT s.sname, COUNT(sc.cno) FROM student s
LEFT JOIN sc ON s.sno = sc.sno
GROUP BY s.sname ORDER BY COUNT(sc.cno) DESC;
-- 预期: 每个学生的选课数量，按降序排列
-- 测试: JOIN → GROUP BY → ORDER BY 全链路

-- 7.4 点号 + 别名 + WHERE + ORDER BY
SELECT s.sno, s.sname, s.classno FROM student s
WHERE s.ssex = 'M' ORDER BY s.sno;
-- 测试: 点号引用在 WHERE 和 ORDER BY 中都能工作


-- ============================================================
-- 八、边界和异常测试
-- ============================================================

-- 8.1 空结果集
SELECT * FROM student WHERE sname = 'Nobody';
-- 预期: Empty set 或 0 rows

-- 8.2 单个常量列（无 FROM，不测试——当前解析器要求有 FROM）

-- 8.3 WHERE 中括号嵌套
SELECT * FROM student WHERE (ssex = 'M' AND classno = 'Rj0801') OR (ssex = 'F' AND classno = 'Rj0803');
-- 测试: 括号内的 AND 组合

-- 8.4 三表链式 LEFT JOIN
SELECT s.sname, c.cname, sc.grade
FROM student s
LEFT JOIN sc ON s.sno = sc.sno
LEFT JOIN course c ON sc.cno = c.cno
ORDER BY s.sname;
-- 测试: 三表 JOIN + ORDER BY

-- 8.5 LIKE '%%' 匹配所有
SELECT * FROM student WHERE sname LIKE '%';
-- 测试: LIKE 全通配


-- ============================================================
-- 九、算术运算 【新功能】
-- ============================================================

-- 9.1 加减法在 SELECT 列表
SELECT sno, grade, (grade + 5) AS adjusted FROM sc;
-- 预期: 每行成绩加 5 分的新列
-- 测试: SELECT 列表中的 + 运算

-- 9.2 减法在 WHERE
SELECT * FROM sc WHERE (grade - 10) < 70;
-- 测试: WHERE 中的算术表达式

-- 9.3 乘除法
SELECT sno, grade, (grade * 1.1) AS scaled FROM sc;
-- 测试: 乘法和浮点运算

-- 9.4 混合运算 + 别名
SELECT sno, cno, ((grade + 10) / 2) AS normalized FROM sc;
-- 测试: 括号内的复合算术表达式


-- ============================================================
-- 十、子查询 【新功能】
-- ============================================================

-- 10.1 EXISTS：有选课记录的学生
SELECT sname FROM student s WHERE EXISTS (SELECT * FROM sc WHERE sc.sno = s.sno);
-- 预期: Jack, Amy, 胡贤斌(如果有sc记录), Mike
-- 测试: EXISTS 关联子查询

-- 10.2 NOT EXISTS：没有选课记录的学生
SELECT sname FROM student s WHERE NOT EXISTS (SELECT * FROM sc WHERE sc.sno = s.sno);
-- 预期: Tom, Lucy, Rose, John（无 sc 记录的学生）
-- 测试: NOT EXISTS 关联子查询

-- 10.3 IN 子查询：成绩 > 80 的学生姓名
SELECT sname FROM student WHERE sno IN (SELECT sno FROM sc WHERE grade > 80);
-- 预期: Jack(88), Amy(91), Mike(95)
-- 测试: IN + 子查询

-- 10.4 NOT IN 子查询：没有选课的学生
SELECT sname FROM student WHERE sno NOT IN (SELECT sno FROM sc);
-- 预期: 所有不在 sc 表中的学生
-- 测试: NOT IN + 子查询

-- 10.5 标量子查询（=）
SELECT sname FROM student WHERE sno = (SELECT sno FROM sc WHERE grade = 95);
-- 预期: Mike（sc 中 grade=95 对应的学号）
-- 测试: 标量子查询用于等值比较

-- 10.6 标量子查询（>）：成绩高于平均分的学生
SELECT sno, grade FROM sc WHERE grade > (SELECT AVG(grade) FROM sc);
-- 预期: 成绩大于平均值的记录（当前平均值约 79.5）
-- 测试: 标量子查询 + 聚合函数

-- 10.7 子查询中的 GROUP BY
SELECT sno FROM sc WHERE sno IN (SELECT sno FROM sc GROUP BY sno HAVING COUNT(*) >= 1);
-- 预期: 所有有选课记录的学生
-- 测试: 子查询内部包含 GROUP BY + HAVING

-- 10.8 NOT IN 子查询 + JOIN 组合
SELECT s.sname FROM student s WHERE s.sno NOT IN (SELECT sno FROM sc) ORDER BY s.sname;
-- 测试: NOT IN 子查询 + ORDER BY 组合

-- 10.9 NOT 与 EXISTS 组合前的逻辑验证
SELECT * FROM student WHERE classno NOT IN ('Rj0801', 'Rj0802');
-- 测试: NOT IN 值列表（非子查询，验证 parseNot 不影响原逻辑）

-- 10.10 子查询嵌套（伪）
SELECT * FROM student WHERE classno IN (SELECT classno FROM class WHERE studentnumber > 24)
AND ssex = 'M';
-- 预期: 学生数 >24 的班级中的男同学（Rj0802(26), Rj0803(25)）
-- 测试: 子查询 + AND 条件组合
+