add_library(columnar_exec_expression_regex_replace STATIC regex_replace.cpp)

target_include_directories(columnar_exec_expression_regex_replace
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(columnar_exec_expression_regex_replace
    PUBLIC
    columnar_core
    columnar_exec_core
    columnar_exec_interface
    re2::re2
)

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/ut/CMakeLists.txt)
    add_subdirectory(ut)
endif()
