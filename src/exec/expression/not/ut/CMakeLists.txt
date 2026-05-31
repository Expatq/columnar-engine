columnar_add_ut(not_ut not_ut.cpp)

target_link_libraries(not_ut
    PRIVATE
    columnar_exec_expression_not
    columnar_exec_expression_logical
    columnar_exec_expression_comparison
    columnar_exec_expression_column_ref
    columnar_exec_expression_literal
)
