columnar_add_ut(date_trunc_ut date_trunc_ut.cpp)

target_link_libraries(date_trunc_ut
    PRIVATE
    columnar_exec_expression_date_trunc
    columnar_exec_expression_column_ref
)
