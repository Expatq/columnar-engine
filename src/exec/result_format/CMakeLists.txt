add_library(
    columnar_exec_result_format
    STATIC
    row_group_builder.cpp
)

target_include_directories(
    columnar_exec_result_format
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(
    columnar_exec_result_format
    PUBLIC
    columnar_core
    columnar_exec_core
)
