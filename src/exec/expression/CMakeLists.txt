add_subdirectory(literal)
add_subdirectory(column_ref)
add_subdirectory(comparison)
add_subdirectory(not)
add_subdirectory(string_len)
add_subdirectory(date_trunc)
add_subdirectory(extract)
add_subdirectory(arithmetic)
add_subdirectory(case_when)
add_subdirectory(logical)
add_subdirectory(like)
add_subdirectory(regex_replace)

add_library(columnar_exec_expression INTERFACE)

target_link_libraries(columnar_exec_expression
    INTERFACE
    columnar_exec_expression_literal
    columnar_exec_expression_column_ref
    columnar_exec_expression_comparison
    columnar_exec_expression_not
    columnar_exec_expression_string_len
    columnar_exec_expression_date_trunc
    columnar_exec_expression_extract
    columnar_exec_expression_arithmetic
    columnar_exec_expression_case_when
    columnar_exec_expression_logical
    columnar_exec_expression_like
    columnar_exec_expression_regex_replace
)
