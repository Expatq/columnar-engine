columnar_add_ut(case_when_ut case_when_ut.cpp)

target_link_libraries(case_when_ut
    PRIVATE
    columnar_exec_expression_case_when
    columnar_exec_expression_comparison
    columnar_exec_expression_column_ref
    columnar_exec_expression_literal
)
