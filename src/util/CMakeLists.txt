add_library(
    columnar_util
    SHARED
    str.cpp
    timer.cpp
)

target_include_directories(columnar_util PUBLIC ${CMAKE_SOURCE_DIR}/src)
