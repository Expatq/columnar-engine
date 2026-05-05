add_subdirectory(global)
add_subdirectory(groupby)
add_subdirectory(lib)

add_library(columnar_exec_aggregate INTERFACE)

target_link_libraries(
    columnar_exec_aggregate
    INTERFACE
    columnar_exec_aggregate_global
    columnar_exec_aggregate_groupby
)
