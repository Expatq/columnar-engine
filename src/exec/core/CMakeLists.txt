add_library(
    columnar_exec_core
    STATIC
    operator_runner.cpp
    selection_vector.cpp
)

target_include_directories(
    columnar_exec_core
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(
    columnar_exec_core
    PUBLIC
    columnar_core
    columnar_exec_interface
    absl::inlined_vector
)
