add_library(columnar_exec_expression_arithmetic STATIC arithmetic.cpp)

target_include_directories(columnar_exec_expression_arithmetic
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(columnar_exec_expression_arithmetic
    PUBLIC
    columnar_core
    columnar_exec_core
    columnar_exec_interface
    columnar_exec_expression_column_ref
    columnar_exec_expression_literal
    absl::flat_hash_set
)

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/ut/CMakeLists.txt)
    add_subdirectory(ut)
endif()
