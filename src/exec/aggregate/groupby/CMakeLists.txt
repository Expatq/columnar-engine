add_library(
    columnar_exec_aggregate_groupby
    STATIC
    aggregation.cpp
    serializer.cpp
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
    absl::flat_hash_map
    absl::inlined_vector
    absl::span
)

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/ut/CMakeLists.txt)
    add_subdirectory(ut)
endif()
