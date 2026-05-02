add_library(
    columnar_exec_aggregate
    STATIC
    spec.cpp
    global_aggregation.cpp
    consumers.cpp
)

target_include_directories(
    columnar_exec_aggregate
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(
    columnar_exec_aggregate
    PUBLIC
    columnar_core
    columnar_exec_core
    columnar_exec_interface
    columnar_exec_expression
    columnar_exec_result_format
)
