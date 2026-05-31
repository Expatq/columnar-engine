columnar_add_ut(logical_ut logical_ut.cpp)

target_link_libraries(logical_ut
    PRIVATE
    columnar_exec_expression_logical
    columnar_exec_expression_comparison
    columnar_exec_expression_column_ref
    columnar_exec_expression_literal
)
