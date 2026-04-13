add_library(
    binary_io
    STATIC
    binary_io.cpp
)

target_include_directories(binary_io PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(binary_io PUBLIC columnar_parser columnar_core)
