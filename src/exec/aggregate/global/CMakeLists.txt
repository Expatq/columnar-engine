add_library(
    columnar_exec_aggregate_global
    STATIC
    aggregation.cpp
)

target_include_directories(
    columnar_exec_aggregate_global
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(
    columnar_exec_aggregate_global
    PUBLIC
    columnar_core
    columnar_exec_core
    columnar_exec_interface
    columnar_exec_expression
    columnar_exec_result_format
    columnar_exec_aggregate_lib
    absl::inlined_vector
)
