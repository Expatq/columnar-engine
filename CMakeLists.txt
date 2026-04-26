cmake_minimum_required(VERSION 4.1)

project(columnar_engine)

include(cmake/compile.cmake)
include(cmake/dependencies.cmake)

add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(tests)
add_subdirectory(bench)
