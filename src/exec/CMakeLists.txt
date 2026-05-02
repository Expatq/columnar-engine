add_subdirectory(interface)
add_subdirectory(core)
add_subdirectory(result_format)
add_subdirectory(source)
add_subdirectory(expression)
add_subdirectory(filter)
add_subdirectory(aggregate)
add_subdirectory(query)

add_library(columnar_exec INTERFACE)

target_link_libraries(
    columnar_exec
    INTERFACE
    columnar_exec_interface
    columnar_exec_core
    columnar_exec_result_format
    columnar_exec_source
    columnar_exec_expression
    columnar_exec_filter
    columnar_exec_aggregate
    columnar_exec_query
)
