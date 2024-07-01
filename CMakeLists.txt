cmake_minimum_required(VERSION 3.10)

project(mx-ui VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)

set(CMAKE_CXX_FLAGS "-O3 -D_REENTRANT -Wall -Wextra -Wpedantic")

SET(CMAKE_EXPORT_COMPILE_COMMANDS true)

#include_directories(/usr/include)
#link_libraries()

add_executable(mx-ui
    src/main.cpp
)
