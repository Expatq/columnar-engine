#include "clickbench_queries.h"

#include <exec/aggregate/global_aggregation.h>
#include <exec/aggregate/spec.h>
#include <exec/core/required_columns.h>
#include <exec/expression/column_ref.h>
#include <exec/expression/comparison.h>
#include <exec/expression/literal.h>
#include <exec/filter/filter.h>
#include <exec/source/metadata_scan.h>
#include <exec/source/table_scan.h>
#include <memory>

namespace Columnar::Exec {

// Q1: SELECT COUNT(*) FROM hits
std::unique_ptr<IOperator> BuildQ1(const std::string& iyxPath) {
    return std::make_unique<MetadataScan>(
        iyxPath,
        std::vector<MetadataColumn>{{MetadataField::TotalRowCount, "count", Types::LogicalType::INT64}});
}

// Q2: SELECT COUNT(*) FROM hits WHERE AdvEngine <> 0
std::unique_ptr<IOperator> BuildQ2(const std::string& iyxPath) {
    auto scan = std::make_unique<TableScan>(
        iyxPath, RequiredColumns::Only({"AdvEngineID"}));

    auto filter = std::make_unique<Filter>(
        std::move(scan),
        std::make_unique<ComparisonExpression>(
            std::make_unique<ColumnRefExpression>("AdvEngineID", Types::LogicalType::INT32),
            CompareOp::NotEq,
            std::make_unique<LiteralExpression>(
                Types::AnyPhysicalType{int32_t{0}}, Types::LogicalType::INT32)));

    std::vector<AggregateSpec> aggs;
    aggs.push_back(CountStar("count"));

    return std::make_unique<GlobalAggregation>(std::move(filter), std::move(aggs));
}

// Q3: SELECT SUM(AdvEngineID), COUNT(*), AVG(ResolutionWidth) FROM hits
std::unique_ptr<IOperator> BuildQ3(const std::string& iyxPath) {
    auto scan = std::make_unique<TableScan>(
        iyxPath, RequiredColumns::Only({"AdvEngineID", "ResolutionWidth"}));

    std::vector<AggregateSpec> aggs;
    aggs.push_back(Sum(
        std::make_unique<ColumnRefExpression>("AdvEngineID", Types::LogicalType::INT32),
        "sum_adv_engine_id",
        Types::LogicalType::INT64));
    aggs.push_back(CountStar("count"));
    aggs.push_back(Avg(
        std::make_unique<ColumnRefExpression>("ResolutionWidth", Types::LogicalType::INT32),
        "avg_resolution_width",
        Types::LogicalType::INT64));

    return std::make_unique<GlobalAggregation>(std::move(scan), std::move(aggs));
}

// Q4: SELECT AVG(UserID) FROM hits
std::unique_ptr<IOperator> BuildQ4(const std::string& iyxPath) {
    auto scan = std::make_unique<TableScan>(
        iyxPath, RequiredColumns::Only({"UserID"}));

    std::vector<AggregateSpec> aggs;
    aggs.push_back(Avg(
        std::make_unique<ColumnRefExpression>("UserID", Types::LogicalType::INT64),
        "avg_user_id",
        Types::LogicalType::INT64));

    return std::make_unique<GlobalAggregation>(std::move(scan), std::move(aggs));
}

}  // namespace Columnar::Exec
