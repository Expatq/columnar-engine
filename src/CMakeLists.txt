add_subdirectory(core)
add_subdirectory(parser)
add_subdirectory(io)
add_subdirectory(util)

add_library(columnar INTERFACE)

target_link_libraries(
    columnar
    INTERFACE
    columnar_core
    columnar_parser
    columnar_io
    columnar_util
)
