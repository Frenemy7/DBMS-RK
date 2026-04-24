cmake_minimum_required(VERSION 3.16)

project(RuanKoDBMS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(client_ui
    client/ui/main.cpp
    client/ui/MainWindow.h
    client/ui/MainWindow.cpp
    client/ui/ConnectDialog.h
    client/ui/ConnectDialog.cpp
)

target_include_directories(client_ui PRIVATE
    client/ui
)

target_link_libraries(client_ui PRIVATE
    Qt6::Widgets
)