add_library(
    format_io
    STATIC
    format_reader.cpp
    format_writer.cpp
)

target_include_directories(format_io PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(format_io PUBLIC binary_io)
