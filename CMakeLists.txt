cmake_minimum_required(VERSION 3.10)
project(miniDBMS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 头文件路径
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/server/include
    ${CMAKE_CURRENT_SOURCE_DIR}/server/src
)

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
add_compile_definitions("$<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>")

# 自动收集所有 .cpp 文件（递归搜索 server 目录）
file(GLOB_RECURSE SOURCES
    server/*.cpp
    server/*.c
)

# 生成可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})