add_library(columnar_exec_expression_logical STATIC logical.cpp)

target_include_directories(columnar_exec_expression_logical
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(columnar_exec_expression_logical
    PUBLIC
    columnar_core
    columnar_exec_core
    columnar_exec_interface
    absl::inlined_vector
    absl::flat_hash_set
)

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/ut/CMakeLists.txt)
    add_subdirectory(ut)
endif()
