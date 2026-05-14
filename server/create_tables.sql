CREATE TABLE student (sno VARCHAR(8) PRIMARY KEY, sname VARCHAR(8) NOT NULL, ssex VARCHAR(2), sbirthday DATETIME, classno VARCHAR(6), totalcredit INT);
CREATE TABLE class (classno VARCHAR(6) PRIMARY KEY, classname VARCHAR(20) NOT NULL, classmajor VARCHAR(20), classdept VARCHAR(20), studentnumber INT);
CREATE TABLE course (cno VARCHAR(6) PRIMARY KEY, cname VARCHAR(30) NOT NULL, ccredit INT);
CREATE TABLE sc (sno VARCHAR(8) NOT NULL, cno VARCHAR(6) NOT NULL, grade INT);
CREATE TABLE teacher (tno INT PRIMARY KEY, tname VARCHAR(8) NOT NULL, tsex VARCHAR(2), tbirthday DATETIME, ttitle VARCHAR(20));
CREATE TABLE teaching (tno INT NOT NULL, cno VARCHAR(6) NOT NULL, language VARCHAR(10));
