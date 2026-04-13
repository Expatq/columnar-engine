add_library(
    csv_io
    STATIC
    csv_reader.cpp
    csv_writer.cpp
)

target_include_directories(csv_io PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(csv_io PUBLIC binary_io)
