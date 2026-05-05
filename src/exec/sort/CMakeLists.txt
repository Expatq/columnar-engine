add_library(columnar_exec_sort STATIC top_k.cpp sort.cpp limit.cpp)

target_include_directories(columnar_exec_sort PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(columnar_exec_sort
    PUBLIC
    columnar_core
    columnar_exec_core
    columnar_exec_interface
    columnar_exec_result_format
    columnar_util
)
