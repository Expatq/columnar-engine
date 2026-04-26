add_subdirectory(csv)
add_subdirectory(format)

add_library(columnar_parser INTERFACE)

target_include_directories(columnar_parser INTERFACE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(columnar_parser INTERFACE csv_parser format_parser)
