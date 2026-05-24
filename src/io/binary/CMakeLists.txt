add_subdirectory(mmap_file)

add_library(
    binary_io
    STATIC
    file_reader.cpp
    file_writer.cpp
)

target_include_directories(binary_io PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(binary_io PUBLIC columnar_parser columnar_core format_lib)
