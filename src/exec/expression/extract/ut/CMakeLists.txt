columnar_add_ut(extract_ut extract_ut.cpp)

target_link_libraries(extract_ut
    PRIVATE
    columnar_exec_expression_extract
    columnar_exec_expression_column_ref
)
