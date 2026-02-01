add_executable(e2e_tests e2e_test.cpp)

target_include_directories(e2e_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(e2e_tests
    PRIVATE
    columnar_io
    columnar_parser
    columnar_util
    GTest::gtest_main
)

gtest_discover_tests(e2e_tests)
