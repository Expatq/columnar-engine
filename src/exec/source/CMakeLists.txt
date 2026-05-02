add_library(
    columnar_exec_source
    STATIC
    metadata_scan.cpp
    table_scan.cpp
)

target_include_directories(
    columnar_exec_source
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(
    columnar_exec_source
    PUBLIC
    columnar_io
    columnar_exec_core
    columnar_exec_result_format
)
