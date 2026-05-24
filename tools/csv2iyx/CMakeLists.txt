add_executable(csv2iyx
    main.cpp
    converter.cpp
)
target_link_libraries(csv2iyx PRIVATE columnar)
