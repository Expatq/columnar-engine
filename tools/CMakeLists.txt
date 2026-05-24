add_subdirectory(csv2iyx)

add_executable(iyx2csv iyx2csv.cpp)
target_link_libraries(iyx2csv PRIVATE columnar)

add_executable(data_generator data_generator.cpp)
target_link_libraries(data_generator PRIVATE columnar)

add_executable(run_query run_query.cpp)
target_link_libraries(run_query PRIVATE columnar)
