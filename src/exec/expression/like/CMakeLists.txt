add_library(columnar_exec_expression_like STATIC like.cpp)

target_include_directories(columnar_exec_expression_like
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(columnar_exec_expression_like
    PUBLIC
    columnar_core
    columnar_exec_core
    columnar_exec_interface
    absl::inlined_vector
)

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/ut/CMakeLists.txt)
    add_subdirectory(ut)
endif()
