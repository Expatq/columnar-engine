columnar_add_ut(top_k_ut top_k_ut.cpp)

target_link_libraries(top_k_ut
    PRIVATE
    columnar_exec_sort
    columnar_exec_expression_column_ref
)
