cmake_minimum_required(VERSION 3.16)

project(RuanKoDBMS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS
    server/src/*.cpp
    server/include/network/*.cpp
)

add_executable(miniDBMS
    server/main.cpp
    ${SERVER_SOURCES}
)

target_include_directories(miniDBMS PRIVATE
    server
    server/include
    server/src
)

target_link_libraries(miniDBMS PRIVATE
    Qt6::Core
    Qt6::Network
)

add_executable(dbms_server
    server/network_main.cpp
    ${SERVER_SOURCES}
)

target_include_directories(dbms_server PRIVATE
    server
    server/include
    server/src
)

target_link_libraries(dbms_server PRIVATE
    Qt6::Core
    Qt6::Network
)

add_executable(client_ui
    client/ui/main.cpp
    client/ui/MainWindow.h
    client/ui/MainWindow.cpp
    client/ui/ConnectDialog.h
    client/ui/ConnectDialog.cpp
    client/network/DbClient.h
    client/network/DbClient.cpp
)

target_include_directories(client_ui PRIVATE
    client/ui
    client/network
)

target_link_libraries(client_ui PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

if(WIN32)
    target_compile_definitions(miniDBMS PRIVATE NOMINMAX)
    target_compile_definitions(dbms_server PRIVATE NOMINMAX)
    target_compile_definitions(client_ui PRIVATE NOMINMAX)
endif()
