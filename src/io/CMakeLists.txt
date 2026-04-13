add_subdirectory(binary)
add_subdirectory(csv)
add_subdirectory(format)

add_library(columnar_io INTERFACE)

target_link_libraries(
    columnar_io 
    INTERFACE
    csv_io
    format_io
)
