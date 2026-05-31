add_library(columnar_exec_expression_extract STATIC extract.cpp)

target_include_directories(columnar_exec_expression_extract
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(columnar_exec_expression_extract
    PUBLIC
    columnar_core
    columnar_exec_core
    columnar_exec_interface
)

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/ut/CMakeLists.txt)
    add_subdirectory(ut)
endif()
