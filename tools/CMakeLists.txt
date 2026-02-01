add_executable(csv2iyx csv2iyx.cpp)
target_link_libraries(csv2iyx PRIVATE columnar)

add_executable(iyx2csv iyx2csv.cpp)
target_link_libraries(iyx2csv PRIVATE columnar)

