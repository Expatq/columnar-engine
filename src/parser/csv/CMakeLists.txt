add_library(
    csv_parser
    STATIC
    csv_parser.cpp
)

target_include_directories(csv_parser PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(csv_parser PUBLIC columnar_core columnar_util)
