add_executable(roundtrip_bench roundtrip_bench.cpp)

target_link_libraries(roundtrip_bench
    PRIVATE
    columnar_io
    columnar_parser
    columnar_util
)

add_executable(clickbench_bench clickbench_bench.cpp)

target_link_libraries(clickbench_bench
    PRIVATE
    columnar_exec_query
    columnar_io
    columnar_parser
    columnar_util
)
