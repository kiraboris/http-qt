cmake_minimum_required(VERSION 3.16)
project(http-qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network)
qt_standard_project_setup()

add_executable(http-qt
    main.cpp
    http_server.cpp
    http_server.h
)

target_link_libraries(http-qt PRIVATE
    Qt6::Core
    Qt6::Network
    pthread
) 