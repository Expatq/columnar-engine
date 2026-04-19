add_library(
    format_parser
    STATIC
    schema_parser.cpp
    serialize_to_string.cpp
    value_parser.cpp
)

target_include_directories(format_parser PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(format_parser PUBLIC columnar_core columnar_util)
