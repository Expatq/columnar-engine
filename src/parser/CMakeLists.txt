add_library(
    columnar_parser
    SHARED
    csv_parser.cpp
    schema_parser.cpp
    value_parser.cpp
)

target_include_directories(columnar_parser PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(columnar_parser PUBLIC columnar_core columnar_util)
