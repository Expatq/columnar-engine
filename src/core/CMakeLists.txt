add_library(
    columnar_core
    STATIC
    types.cpp
    column.cpp
    schema.cpp
    batch.cpp
    row_group.cpp
)

target_include_directories(columnar_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
