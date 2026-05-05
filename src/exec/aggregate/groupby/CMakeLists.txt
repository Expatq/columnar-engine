add_library(
    columnar_exec_aggregate_groupby
    STATIC
    aggregation.cpp
)

target_include_directories(
    columnar_exec_aggregate_groupby
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(
    columnar_exec_aggregate_groupby
    PUBLIC
    columnar_core
    columnar_exec_core
    columnar_exec_interface
    columnar_exec_expression
    columnar_exec_result_format
    columnar_exec_aggregate_lib
)
