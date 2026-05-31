columnar_add_ut(column_ref_ut column_ref_ut.cpp)

target_link_libraries(column_ref_ut
    PRIVATE
    columnar_exec_expression_column_ref
)
