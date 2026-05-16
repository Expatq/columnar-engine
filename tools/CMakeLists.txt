add_executable(csv2iyx csv2iyx.cpp)
target_link_libraries(csv2iyx PRIVATE columnar columnar_tcmalloc)

add_executable(iyx2csv iyx2csv.cpp)
target_link_libraries(iyx2csv PRIVATE columnar columnar_tcmalloc)

add_executable(data_generator data_generator.cpp)
target_link_libraries(data_generator PRIVATE columnar columnar_tcmalloc)

add_executable(run_query run_query.cpp)
target_link_libraries(run_query PRIVATE columnar columnar_exec_query columnar_tcmalloc)
