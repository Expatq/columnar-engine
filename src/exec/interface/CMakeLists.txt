add_library(columnar_exec_interface INTERFACE)

target_include_directories(
    columnar_exec_interface
    INTERFACE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(
    columnar_exec_interface
    INTERFACE
    columnar_core
)
