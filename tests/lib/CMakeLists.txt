add_library(columnar_test_lib STATIC
    data_builders.cpp
    exec_batch_builders.cpp
    fake_operator.cpp
)

target_include_directories(columnar_test_lib
    PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(columnar_test_lib
    PUBLIC
    columnar_core
    columnar_exec_core
    columnar_exec_interface
)
