columnar_add_ut(aggregate_state_ut state_ut.cpp)

target_link_libraries(aggregate_state_ut
    PRIVATE
    columnar_exec_aggregate_lib
    columnar_exec_result_format
)

columnar_add_ut(aggregate_spec_ut spec_ut.cpp)

target_link_libraries(aggregate_spec_ut
    PRIVATE
    columnar_exec_aggregate_lib
    columnar_exec_expression_column_ref
)
