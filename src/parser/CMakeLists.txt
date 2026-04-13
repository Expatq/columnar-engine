add_subdirectory(csv)
add_subdirectory(format)

add_library(parser INTERFACE)
target_link_directories(
    parser
    INTERFACE
    csv_parser
    format_parser
)
