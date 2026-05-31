#include <exec/expression/case_when/case_when.h>

#include <exec/expression/column_ref/column_ref.h>
#include <exec/expression/comparison/comparison.h>
#include <exec/expression/literal/literal.h>

#include <exec/core/exec_batch.h>
#include <exec/interface/expression.h>

#include <tests/lib/data_builders.h>
#include <tests/lib/exec_batch_builders.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Columnar::Test {

namespace {

constexpr auto kI32 = Types::LogicalType::INT32;
constexpr auto kI64 = Types::LogicalType::INT64;
constexpr auto kStr = Types::LogicalType::STRING;
constexpr auto kDate = Types::LogicalType::DATE;

std::unique_ptr<Exec::IExpression> Col(std::string name, Types::LogicalType type) {
    return std::make_unique<Exec::ColumnRefExpression>(std::move(name), type);
}

template <typename T>
std::unique_ptr<Exec::IExpression> Lit(T value, Types::LogicalType type) {
    return std::make_unique<Exec::LiteralExpression>(Types::AnyPhysicalType{value}, type);
}

std::unique_ptr<Exec::IExpression> CmpEq(std::string col, int64_t value) {
    return std::make_unique<Exec::ComparisonExpression>(
        Col(std::move(col), kI64), Exec::CompareOp::Eq, Lit<int64_t>(value, kI64));
}

std::unique_ptr<Exec::IExpression> SelfEq(std::string col) {
    return std::make_unique<Exec::ComparisonExpression>(
        Col(col, kI64), Exec::CompareOp::Eq, Col(std::move(col), kI64));
}

template <typename T>
std::span<const T> AsSpan(const Exec::ColumnSpan& span) {
    return std::get<std::span<const T>>(span);
}

class FixtureCaseWhen : public ::testing::Test {
protected:
    Exec::EvalState state_;
};

}  // namespace

TEST_F(FixtureCaseWhen, KindReportsCaseWhen) {
    Exec::CaseWhenExpression expr(CmpEq("x", 1), Lit<int64_t>(10, kI64), Lit<int64_t>(20, kI64));

    EXPECT_EQ(expr.Kind(), Exec::ExpressionKind::CaseWhen);
}

TEST_F(FixtureCaseWhen, ResultTypeMatchesBranches) {
    Exec::CaseWhenExpression expr(CmpEq("x", 1), Lit<int64_t>(10, kI64), Lit<int64_t>(20, kI64));

    EXPECT_EQ(expr.ResultType(), kI64);
}

TEST_F(FixtureCaseWhen, RequiredColumnsUnionsAndDeduplicates) {
    Exec::CaseWhenExpression expr(
        CmpEq("x", 1),
        Col("y", kI64),
        Col("x", kI64));

    auto cols = expr.RequiredColumns();
    std::sort(cols.begin(), cols.end());

    ASSERT_EQ(cols.size(), 2u);
    EXPECT_EQ(cols[0], "x");
    EXPECT_EQ(cols[1], "y");
}

TEST_F(FixtureCaseWhen, ConstructorRejectsNullCondition) {
    EXPECT_THROW(
        Exec::CaseWhenExpression(nullptr, Lit<int64_t>(1, kI64), Lit<int64_t>(0, kI64)),
        std::invalid_argument);
}

TEST_F(FixtureCaseWhen, ConstructorRejectsNullThen) {
    EXPECT_THROW(
        Exec::CaseWhenExpression(CmpEq("x", 1), nullptr, Lit<int64_t>(0, kI64)),
        std::invalid_argument);
}

TEST_F(FixtureCaseWhen, ConstructorRejectsNullElse) {
    EXPECT_THROW(
        Exec::CaseWhenExpression(CmpEq("x", 1), Lit<int64_t>(1, kI64), nullptr),
        std::invalid_argument);
}

TEST_F(FixtureCaseWhen, ConstructorRejectsNonBoolCondition) {
    EXPECT_THROW(
        Exec::CaseWhenExpression(Col("x", kI64), Lit<int64_t>(1, kI64), Lit<int64_t>(0, kI64)),
        std::invalid_argument);
}

TEST_F(FixtureCaseWhen, ConstructorRejectsLogicalTypeMismatchEvenWhenPhysicalMatches) {
    EXPECT_THROW(
        Exec::CaseWhenExpression(CmpEq("x", 1), Lit<int32_t>(0, kI32), Lit<int32_t>(0, kDate)),
        std::invalid_argument)
        << "INT32 and DATE share physical INT32 but mean different things";
}

TEST_F(FixtureCaseWhen, PicksThenForMatchingRowsAndElseOtherwise) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}, {"hit", kI64}, {"miss", kI64}}),
        MakeColumn<int64_t>({1, 2, 1, 2}),
        MakeColumn<int64_t>({100, 200, 300, 400}),
        MakeColumn<int64_t>({-100, -200, -300, -400})));

    Exec::CaseWhenExpression expr(
        CmpEq("x", 1), Col("hit", kI64), Col("miss", kI64));

    const auto result = AsSpan<int64_t>(expr.EvaluateColumn(batch, state_));

    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], 100);
    EXPECT_EQ(result[1], -200);
    EXPECT_EQ(result[2], 300);
    EXPECT_EQ(result[3], -400);
}

TEST_F(FixtureCaseWhen, ConditionMatchingNoneSelectsElseEverywhere) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}, {"hit", kI64}, {"miss", kI64}}),
        MakeColumn<int64_t>({1, 2, 3}),
        MakeColumn<int64_t>({10, 20, 30}),
        MakeColumn<int64_t>({-1, -2, -3})));

    Exec::CaseWhenExpression expr(
        CmpEq("x", 999), Col("hit", kI64), Col("miss", kI64));

    const auto result = AsSpan<int64_t>(expr.EvaluateColumn(batch, state_));

    EXPECT_EQ(result[0], -1);
    EXPECT_EQ(result[1], -2);
    EXPECT_EQ(result[2], -3);
}

TEST_F(FixtureCaseWhen, ConditionMatchingAllSelectsThenEverywhere) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}, {"hit", kI64}, {"miss", kI64}}),
        MakeColumn<int64_t>({7, 8, 9}),
        MakeColumn<int64_t>({10, 20, 30}),
        MakeColumn<int64_t>({-1, -2, -3})));

    Exec::CaseWhenExpression expr(
        SelfEq("x"), Col("hit", kI64), Col("miss", kI64));

    const auto result = AsSpan<int64_t>(expr.EvaluateColumn(batch, state_));

    EXPECT_EQ(result[0], 10);
    EXPECT_EQ(result[1], 20);
    EXPECT_EQ(result[2], 30);
}

TEST_F(FixtureCaseWhen, StringBranchesPickRefererOrEmpty) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"engine", kI64}, {"referer", kStr}}),
        MakeColumn<int64_t>({0, 1, 0, 2}),
        MakeColumn<std::string>({"http://a/", "http://b/", "http://c/", "http://d/"})));

    Exec::CaseWhenExpression expr(
        CmpEq("engine", 0),
        Col("referer", kStr),
        Lit<std::string>(std::string{""}, kStr));

    const auto result = AsSpan<std::string>(expr.EvaluateColumn(batch, state_));

    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], "http://a/");
    EXPECT_EQ(result[1], "");
    EXPECT_EQ(result[2], "http://c/");
    EXPECT_EQ(result[3], "");
}

TEST_F(FixtureCaseWhen, EvaluateScalarSelectsThenWhenConditionMatches) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}, {"hit", kI64}, {"miss", kI64}}),
        MakeColumn<int64_t>({5, 1, 5}),
        MakeColumn<int64_t>({100, 200, 300}),
        MakeColumn<int64_t>({-1, -2, -3})));

    Exec::CaseWhenExpression expr(
        CmpEq("x", 5), Col("hit", kI64), Col("miss", kI64));

    EXPECT_EQ(std::get<int64_t>(expr.EvaluateScalar(batch, /*row=*/0)), 100);
    EXPECT_EQ(std::get<int64_t>(expr.EvaluateScalar(batch, /*row=*/1)), -2);
    EXPECT_EQ(std::get<int64_t>(expr.EvaluateScalar(batch, /*row=*/2)), 300);
}

TEST_F(FixtureCaseWhen, RespectsInputSelectionVector) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"x", kI64}, {"hit", kI64}, {"miss", kI64}}),
        MakeColumn<int64_t>({1, 2, 1, 2, 1}),
        MakeColumn<int64_t>({10, 20, 30, 40, 50}),
        MakeColumn<int64_t>({-1, -2, -3, -4, -5}));
    auto batch = MakeBatchWithSelection(rowGroup, {1, 2, 4});

    Exec::CaseWhenExpression expr(
        CmpEq("x", 1), Col("hit", kI64), Col("miss", kI64));

    const auto result = AsSpan<int64_t>(expr.EvaluateColumn(batch, state_));

    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], -2);
    EXPECT_EQ(result[1], 30);
    EXPECT_EQ(result[2], 50);
}

TEST_F(FixtureCaseWhen, EmptyBatchReturnsEmptySpan) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}, {"hit", kI64}, {"miss", kI64}}),
        MakeColumn<int64_t>({}),
        MakeColumn<int64_t>({}),
        MakeColumn<int64_t>({})));

    Exec::CaseWhenExpression expr(
        CmpEq("x", 1), Col("hit", kI64), Col("miss", kI64));

    EXPECT_TRUE(AsSpan<int64_t>(expr.EvaluateColumn(batch, state_)).empty());
}

TEST_F(FixtureCaseWhen, EvaluateScalarOnFailingConditionPicksElse) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}, {"a", kI64}, {"b", kI64}}),
        MakeColumn<int64_t>({42}),
        MakeColumn<int64_t>({1}),
        MakeColumn<int64_t>({2})));

    Exec::CaseWhenExpression expr(CmpEq("x", 999), Col("a", kI64), Col("b", kI64));

    EXPECT_EQ(std::get<int64_t>(expr.EvaluateScalar(batch, 0)), 2);
}

}  // namespace Columnar::Test
