add_library(
    columnar_io
    STATIC
    binary_io.cpp
    csv_reader.cpp
    csv_writer.cpp
    format_writer.cpp
    format_reader.cpp
)

target_include_directories(columnar_io PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(columnar_io PUBLIC columnar_core columnar_parser)
