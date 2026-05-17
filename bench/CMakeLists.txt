add_executable(roundtrip_bench roundtrip_bench.cpp)

target_link_libraries(roundtrip_bench
    PRIVATE
    columnar
)

add_executable(clickbench_bench clickbench.cpp)

target_link_libraries(clickbench_bench
    PRIVATE
    columnar
)
