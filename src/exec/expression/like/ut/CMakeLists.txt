columnar_add_ut(like_ut like_ut.cpp)

target_link_libraries(like_ut
    PRIVATE
    columnar_exec_expression_like
    columnar_exec_expression_column_ref
)
