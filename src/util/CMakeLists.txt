add_library(
    columnar_util
    STATIC
    str.cpp
    batch_builder.cpp
    statistics.cpp
)

target_include_directories(columnar_util PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(columnar_util PUBLIC columnar_core)
