columnar_add_ut(literal_ut literal_ut.cpp)

target_link_libraries(literal_ut
    PRIVATE
    columnar_exec_expression_literal
)
