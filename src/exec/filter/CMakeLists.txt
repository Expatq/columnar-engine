add_library(
    columnar_exec_filter
    STATIC
    filter.cpp
)

target_include_directories(
    columnar_exec_filter
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(
    columnar_exec_filter
    PUBLIC
    columnar_exec_core
    columnar_exec_expression
)
