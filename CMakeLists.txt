cmake_minimum_required(VERSION 3.10)

project(mx-ui VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_CXX_FLAGS "-g3 -ggdb -D_REENTRANT -Wall -Wextra -Wpedantic")
set(CMAKE_EXPORT_COMPILE_COMMANDS true)

find_package(PkgConfig)
pkg_check_modules(GTKMM gtkmm-4.0)

include_directories(
    ${GTKMM_INCLUDE_DIRS}
)

link_directories(
    ${GTKMM_LIBRARY_DIRS}
)

link_libraries(
    ${GTKMM_LIBRARIES}
)

add_executable(mx-ui
    src/main.cpp
)
