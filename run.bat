@echo off
:: 切换到脚本所在的当前目录（即项目根目录）
cd /d "%~dp0"
:: 运行 build 文件夹下的 exe 程序
.\build\miniDBMS.exe