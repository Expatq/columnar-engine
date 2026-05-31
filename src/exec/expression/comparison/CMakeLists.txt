add_library(columnar_exec_expression_comparison STATIC comparison.cpp)

target_include_directories(columnar_exec_expression_comparison
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(columnar_exec_expression_comparison
    PUBLIC
    columnar_core
    columnar_exec_core
    columnar_exec_interface
    columnar_exec_expression_column_ref
    columnar_exec_expression_literal
)

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/ut/CMakeLists.txt)
    add_subdirectory(ut)
endif()
