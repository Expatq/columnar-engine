include(GoogleTest)

function(columnar_add_ut name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name}
        PRIVATE
        columnar_test_lib
        GTest::gtest_main
    )
    gtest_discover_tests(${name})
endfunction()
