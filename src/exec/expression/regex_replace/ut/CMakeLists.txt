columnar_add_ut(regex_replace_ut regex_replace_ut.cpp)

target_link_libraries(regex_replace_ut
    PRIVATE
    columnar_exec_expression_regex_replace
    columnar_exec_expression_column_ref
)
