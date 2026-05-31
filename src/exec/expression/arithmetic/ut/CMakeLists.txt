columnar_add_ut(arithmetic_ut arithmetic_ut.cpp)

target_link_libraries(arithmetic_ut
    PRIVATE
    columnar_exec_expression_arithmetic
    columnar_exec_expression_column_ref
    columnar_exec_expression_literal
)
