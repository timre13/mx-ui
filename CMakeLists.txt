cmake_minimum_required(VERSION 3.10)

project(mx-ui VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -g3 -ggdb -D_REENTRANT")
set(CMAKE_EXPORT_COMPILE_COMMANDS true)

find_package(PkgConfig)
pkg_check_modules(GTKMM gtkmm-4.0)
pkg_check_modules(GLIBMM glibmm-2.68)
pkg_check_modules(GIOMM giomm-2.68)

include_directories(
    ${GTKMM_INCLUDE_DIRS}
    ${GLIBMM_INCLUDE_DIRS}
    ${GIOMM_INCLUDE_DIRS}
)

link_directories(
    ${GTKMM_LIBRARY_DIRS}
    ${GLIBMM_LIBRARY_DIRS}
    ${GIOMM_LIBRARY_DIRS}
)

link_libraries(
    ${GTKMM_LIBRARIES}
    ${GLIBMM_LIBRARIES}
    ${GIOMM_LIBRARIES}
)

# https://stackoverflow.com/questions/63697778/how-to-use-glib-compile-resources-with-cmake

find_program(GLIB_COMPILE_RESOURCES NAMES glib-compile-resources REQUIRED)
set(GRESOURCE_OUT mx-ui.gresource.c)
set(GRESOURCE_XML ${PROJECT_SOURCE_DIR}/data/mx-ui.gresource.xml)

add_custom_command(
    OUTPUT ${GRESOURCE_OUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data
    COMMAND ${GLIB_COMPILE_RESOURCES}
    ARGS
        --target=${CMAKE_CURRENT_BINARY_DIR}/${GRESOURCE_OUT}
        --generate-source
        ${GRESOURCE_XML}
    VERBATIM
    MAIN_DEPENDENCY ${GRESOURCE_XML}
    DEPENDS
        data/main.ui
        data/style.css
)

add_custom_target(
    resources
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${GRESOURCE_OUT}
)


set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/${GRESOURCE_OUT}
    PROPERTIES GENERATED TRUE
)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/Frame.cpp
    src/protocol.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/${GRESOURCE_OUT}
)

add_dependencies(${PROJECT_NAME} resources)
