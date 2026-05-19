add_library(
    csv_parallel_io
    STATIC
    csv_boundary_index.cpp
    csv_range_reader.cpp
)

target_include_directories(csv_parallel_io PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(csv_parallel_io PUBLIC binary_io columnar_core)
