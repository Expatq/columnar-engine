cmake_minimum_required(VERSION 3.28)

project(columnar_engine)

include(cmake/compile.cmake)
include(cmake/dependencies.cmake)
include(cmake/allocators.cmake)

add_compile_options(${COLUMNAR_WARNING_FLAGS})

add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(tests)
add_subdirectory(bench)
