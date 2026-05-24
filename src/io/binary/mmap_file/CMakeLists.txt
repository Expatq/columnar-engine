add_library(
    format_lib
    STATIC
    mmap_file.cpp
)

target_include_directories(format_lib PUBLIC ${CMAKE_SOURCE_DIR}/src)
