add_library(
    format_io
    STATIC
    format_reader.cpp
    format_writer.cpp
    bitpack.cpp
)

target_include_directories(format_io PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(format_io PUBLIC binary_io)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_options(format_io PRIVATE -mavx2)
endif()
