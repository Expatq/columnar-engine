columnar_add_ut(inline_key_ut inline_key_ut.cpp)

target_link_libraries(inline_key_ut
    PRIVATE
    columnar_exec_aggregate_groupby
)

columnar_add_ut(groupby_serializer_ut serializer_ut.cpp)

target_link_libraries(groupby_serializer_ut
    PRIVATE
    columnar_exec_aggregate_groupby
    columnar_exec_expression_column_ref
)

columnar_add_ut(groupby_aggregation_ut aggregation_ut.cpp)

target_link_libraries(groupby_aggregation_ut
    PRIVATE
    columnar_exec_aggregate_groupby
    columnar_exec_expression_column_ref
)
