add_library(
    columnar_exec_query
    STATIC
    clickbench_queries.cpp
)

target_include_directories(
    columnar_exec_query
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(
    columnar_exec_query
    PUBLIC
    columnar_exec_source
    columnar_exec_filter
    columnar_exec_expression
    columnar_exec_aggregate
    columnar_exec_sort
)
