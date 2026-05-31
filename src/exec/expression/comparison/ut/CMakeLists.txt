columnar_add_ut(comparison_ut comparison_ut.cpp)

target_link_libraries(comparison_ut
    PRIVATE
    columnar_exec_expression_comparison
    columnar_exec_expression_column_ref
    columnar_exec_expression_literal
    columnar_exec_expression_arithmetic
)
