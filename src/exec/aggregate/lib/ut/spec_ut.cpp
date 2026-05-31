#include <exec/aggregate/lib/spec.h>

#include <exec/expression/column_ref/column_ref.h>
#include <exec/expression/literal/literal.h>

#include <exec/interface/expression.h>

#include <core/types.h>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace Columnar::Test {

namespace {

constexpr auto kI64 = Types::LogicalType::INT64;
constexpr auto kStr = Types::LogicalType::STRING;
constexpr auto kDate = Types::LogicalType::DATE;

std::unique_ptr<Exec::IExpression> Col(std::string name, Types::LogicalType type) {
    return std::make_unique<Exec::ColumnRefExpression>(std::move(name), type);
}

}  // namespace

TEST(AggregateSpec, CountStarHasNoInputAndReturnsInt64) {
    auto spec = Exec::CountStar("cnt");

    EXPECT_EQ(spec.kind, Exec::AggregateKind::CountStar);
    EXPECT_FALSE(spec.HasInput());
    EXPECT_EQ(spec.outputName, "cnt");
    EXPECT_EQ(spec.outputType, kI64);
    EXPECT_THROW(spec.InputType(), std::logic_error);
}

TEST(AggregateSpec, CountColumnRequiresInput) {
    auto spec = Exec::CountColumn(Col("x", kI64), "cnt");

    EXPECT_EQ(spec.kind, Exec::AggregateKind::CountColumn);
    EXPECT_TRUE(spec.HasInput());
    EXPECT_EQ(spec.outputType, kI64);
    EXPECT_EQ(spec.InputType(), kI64);
}

TEST(AggregateSpec, CountColumnRejectsNullInput) {
    EXPECT_THROW(Exec::CountColumn(nullptr, "cnt"), std::invalid_argument);
}

TEST(AggregateSpec, CountDistinctRequiresInputAndReturnsInt64) {
    auto spec = Exec::CountDistinct(Col("x", kStr), "u");

    EXPECT_EQ(spec.kind, Exec::AggregateKind::CountDistinct);
    EXPECT_EQ(spec.outputType, kI64);
    EXPECT_EQ(spec.InputType(), kStr);
}

TEST(AggregateSpec, CountDistinctRejectsNullInput) {
    EXPECT_THROW(Exec::CountDistinct(nullptr, "u"), std::invalid_argument);
}

TEST(AggregateSpec, SumRespectsCallerProvidedOutputType) {
    auto spec = Exec::Sum(Col("x", kI64), "s", kI64);

    EXPECT_EQ(spec.kind, Exec::AggregateKind::Sum);
    EXPECT_EQ(spec.outputType, kI64);
    EXPECT_EQ(spec.InputType(), kI64);
}

TEST(AggregateSpec, SumRejectsNullInput) {
    EXPECT_THROW(Exec::Sum(nullptr, "s", kI64), std::invalid_argument);
}

TEST(AggregateSpec, AvgRespectsCallerProvidedOutputType) {
    auto spec = Exec::Avg(Col("x", kI64), "a", kI64);

    EXPECT_EQ(spec.kind, Exec::AggregateKind::Avg);
    EXPECT_EQ(spec.outputType, kI64);
}

TEST(AggregateSpec, AvgRejectsNullInput) {
    EXPECT_THROW(Exec::Avg(nullptr, "a", kI64), std::invalid_argument);
}

TEST(AggregateSpec, MinDerivesOutputTypeFromInput) {
    auto spec = Exec::Min(Col("dt", kDate), "min_dt");

    EXPECT_EQ(spec.kind, Exec::AggregateKind::Min);
    EXPECT_EQ(spec.outputType, kDate)
        << "Min preserves logical type of input (DATE stays DATE, not INT32)";
}

TEST(AggregateSpec, MaxDerivesOutputTypeFromInput) {
    auto spec = Exec::Max(Col("s", kStr), "max_s");

    EXPECT_EQ(spec.kind, Exec::AggregateKind::Max);
    EXPECT_EQ(spec.outputType, kStr);
}

TEST(AggregateSpec, MinRejectsNullInput) {
    EXPECT_THROW(Exec::Min(nullptr, "x"), std::invalid_argument);
}

TEST(AggregateSpec, MaxRejectsNullInput) {
    EXPECT_THROW(Exec::Max(nullptr, "x"), std::invalid_argument);
}

TEST(AggregateSpec, OutputNameIsPreserved) {
    auto spec = Exec::Sum(Col("x", kI64), "total_widgets", kI64);

    EXPECT_EQ(spec.outputName, "total_widgets");
}

TEST(AggregateSpec, MoveConstructibleAndMoveAssignable) {
    auto first = Exec::Sum(Col("x", kI64), "a", kI64);
    auto second = std::move(first);

    EXPECT_EQ(second.outputName, "a");
    EXPECT_EQ(second.kind, Exec::AggregateKind::Sum);
}

}  // namespace Columnar::Test
