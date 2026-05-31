columnar_add_ut(string_len_ut string_len_ut.cpp)

target_link_libraries(string_len_ut
    PRIVATE
    columnar_exec_expression_string_len
    columnar_exec_expression_column_ref
)
