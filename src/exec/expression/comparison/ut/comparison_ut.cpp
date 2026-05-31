#include <exec/expression/comparison/comparison.h>

#include <exec/expression/arithmetic/arithmetic.h>
#include <exec/expression/column_ref/column_ref.h>
#include <exec/expression/literal/literal.h>

#include <tests/lib/data_builders.h>
#include <tests/lib/exec_batch_builders.h>

#include <util/int128.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace Columnar::Test {

namespace {

constexpr auto kI32 = Types::LogicalType::INT32;
constexpr auto kI64 = Types::LogicalType::INT64;
constexpr auto kI128 = Types::LogicalType::INT128;
constexpr auto kStr = Types::LogicalType::STRING;
constexpr auto kBool = Types::LogicalType::BOOL;
constexpr auto kDate = Types::LogicalType::DATE;
constexpr auto kTs = Types::LogicalType::TIMESTAMP;

std::unique_ptr<Exec::IExpression> Col(std::string name, Types::LogicalType type) {
    return std::make_unique<Exec::ColumnRefExpression>(std::move(name), type);
}

template <typename T>
std::unique_ptr<Exec::IExpression> Lit(T value, Types::LogicalType type) {
    return std::make_unique<Exec::LiteralExpression>(Types::AnyPhysicalType{value}, type);
}

std::vector<Exec::RowId> AsVec(const Exec::SelectionVector& sel) {
    return {sel.Rows().begin(), sel.Rows().end()};
}

class FixtureComparison : public ::testing::Test {
protected:
    Exec::SelectionVector output_;
};

}  // namespace

TEST_F(FixtureComparison, KindReportsComparison) {
    Exec::ComparisonExpression expr(Col("x", kI64), Exec::CompareOp::Eq, Lit<int64_t>(5, kI64));

    EXPECT_EQ(expr.Kind(), Exec::ExpressionKind::Comparison);
}

TEST_F(FixtureComparison, ResultTypeIsBool) {
    Exec::ComparisonExpression expr(Col("x", kI64), Exec::CompareOp::Eq, Lit<int64_t>(5, kI64));

    EXPECT_EQ(expr.ResultType(), kBool);
}

TEST_F(FixtureComparison, RequiredColumnsConcatenatesOperandColumns) {
    Exec::ComparisonExpression expr(Col("a", kI64), Exec::CompareOp::Lt, Col("b", kI64));

    auto cols = expr.RequiredColumns();
    std::sort(cols.begin(), cols.end());

    ASSERT_EQ(cols.size(), 2u);
    EXPECT_EQ(cols[0], "a");
    EXPECT_EQ(cols[1], "b");
}

TEST_F(FixtureComparison, RequiredColumnsKeepsDuplicateForSameColumnOnBothSides) {
    Exec::ComparisonExpression expr(Col("x", kI64), Exec::CompareOp::Eq, Col("x", kI64));

    EXPECT_EQ(expr.RequiredColumns().size(), 2u)
        << "ComparisonExpression does not deduplicate column names";
}

TEST_F(FixtureComparison, ConstructorRejectsNullLeftOperand) {
    EXPECT_THROW(
        Exec::ComparisonExpression(nullptr, Exec::CompareOp::Eq, Lit<int64_t>(1, kI64)),
        std::invalid_argument);
}

TEST_F(FixtureComparison, ConstructorRejectsNullRightOperand) {
    EXPECT_THROW(
        Exec::ComparisonExpression(Col("x", kI64), Exec::CompareOp::Eq, nullptr),
        std::invalid_argument);
}

TEST_F(FixtureComparison, ConstructorRejectsPhysicalTypeMismatchIntegerWidths) {
    EXPECT_THROW(
        Exec::ComparisonExpression(Col("x", kI32), Exec::CompareOp::Eq, Lit<int64_t>(1, kI64)),
        std::invalid_argument);
}

TEST_F(FixtureComparison, ConstructorRejectsPhysicalTypeMismatchStringVsInt) {
    EXPECT_THROW(
        Exec::ComparisonExpression(Col("name", kStr), Exec::CompareOp::Eq, Lit<int64_t>(1, kI64)),
        std::invalid_argument);
}

TEST_F(FixtureComparison, ConstructorAcceptsDifferentLogicalTypesWithMatchingPhysical) {
    EXPECT_NO_THROW(
        Exec::ComparisonExpression(Col("d", kDate), Exec::CompareOp::Eq, Lit<int32_t>(0, kI32)))
        << "DATE and INT32 share PhysicalType::INT32";
}

TEST_F(FixtureComparison, EqColumnLiteralSelectsExactlyMatchingRows) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({3, 5, 7, 5, 1})));

    Exec::ComparisonExpression expr(Col("x", kI64), Exec::CompareOp::Eq, Lit<int64_t>(5, kI64));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{1, 3}));
}

TEST_F(FixtureComparison, NotEqColumnLiteralSelectsRowsThatDifferFromConstant) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({3, 5, 7, 5, 1})));

    Exec::ComparisonExpression expr(Col("x", kI64), Exec::CompareOp::NotEq, Lit<int64_t>(5, kI64));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 2, 4}));
}

TEST_F(FixtureComparison, LtColumnLiteralSelectsStrictlyLessRows) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({3, 5, 7, 5, 1})));

    Exec::ComparisonExpression expr(Col("x", kI64), Exec::CompareOp::Lt, Lit<int64_t>(5, kI64));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 4}));
}

TEST_F(FixtureComparison, LteColumnLiteralIncludesEqualValues) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({3, 5, 7, 5, 1})));

    Exec::ComparisonExpression expr(Col("x", kI64), Exec::CompareOp::Lte, Lit<int64_t>(5, kI64));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 1, 3, 4}));
}

TEST_F(FixtureComparison, GtColumnLiteralSelectsStrictlyGreaterRows) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({3, 5, 7, 5, 1})));

    Exec::ComparisonExpression expr(Col("x", kI64), Exec::CompareOp::Gt, Lit<int64_t>(5, kI64));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{2}));
}

TEST_F(FixtureComparison, GteColumnLiteralIncludesEqualValues) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({3, 5, 7, 5, 1})));

    Exec::ComparisonExpression expr(Col("x", kI64), Exec::CompareOp::Gte, Lit<int64_t>(5, kI64));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{1, 2, 3}));
}

TEST_F(FixtureComparison, EqColumnColumnComparesPositionwise) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"a", kI64}, {"b", kI64}}),
        MakeColumn<int64_t>({1, 2, 3, 4}),
        MakeColumn<int64_t>({1, 0, 3, 5})));

    Exec::ComparisonExpression expr(Col("a", kI64), Exec::CompareOp::Eq, Col("b", kI64));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 2}));
}

TEST_F(FixtureComparison, LtColumnColumnComparesPositionwise) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"a", kI64}, {"b", kI64}}),
        MakeColumn<int64_t>({1, 5, 3, 9}),
        MakeColumn<int64_t>({2, 4, 3, 10})));

    Exec::ComparisonExpression expr(Col("a", kI64), Exec::CompareOp::Lt, Col("b", kI64));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 3}));
}

TEST_F(FixtureComparison, EqColumnColumnSameColumnAlwaysMatches) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3, 4})));

    Exec::ComparisonExpression expr(Col("x", kI64), Exec::CompareOp::Eq, Col("x", kI64));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 1, 2, 3}));
}

TEST_F(FixtureComparison, StringColumnLiteralEqUsesByteIdentity) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"name", kStr}}),
        MakeColumn<std::string>({"alice", "Alice", "alice", ""})));

    Exec::ComparisonExpression expr(
        Col("name", kStr),
        Exec::CompareOp::Eq,
        Lit<std::string>(std::string{"alice"}, kStr));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 2}))
        << "string Eq is case-sensitive byte comparison";
}

TEST_F(FixtureComparison, StringColumnLiteralLtIsLexicographic) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"name", kStr}}),
        MakeColumn<std::string>({"apple", "banana", "apricot", "", "ant"})));

    Exec::ComparisonExpression expr(
        Col("name", kStr),
        Exec::CompareOp::Lt,
        Lit<std::string>(std::string{"apple"}, kStr));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{3, 4}))
        << "empty string and 'ant' precede 'apple' lexicographically";
}

TEST_F(FixtureComparison, StringNotEqMatchesEmptyStringAgainstNonEmpty) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"s", kStr}}),
        MakeColumn<std::string>({"", "x", ""})));

    Exec::ComparisonExpression expr(
        Col("s", kStr),
        Exec::CompareOp::NotEq,
        Lit<std::string>(std::string{""}, kStr));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{1}));
}

TEST_F(FixtureComparison, NegativeIntegersCompareCorrectly) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI32}}),
        MakeColumn<int32_t>({-100, -1, 0, 1, 100})));

    Exec::ComparisonExpression expr(
        Col("x", kI32), Exec::CompareOp::Lt, Lit<int32_t>(0, kI32));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 1}));
}

TEST_F(FixtureComparison, Int32MinAndMaxBoundariesCompareCorrectly) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI32}}),
        MakeColumn<int32_t>({std::numeric_limits<int32_t>::min(),
                             std::numeric_limits<int32_t>::min() + 1,
                             std::numeric_limits<int32_t>::max() - 1,
                             std::numeric_limits<int32_t>::max()})));

    Exec::ComparisonExpression eqMin(
        Col("x", kI32),
        Exec::CompareOp::Eq,
        Lit<int32_t>(std::numeric_limits<int32_t>::min(), kI32));
    eqMin.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0}));

    Exec::ComparisonExpression eqMax(
        Col("x", kI32),
        Exec::CompareOp::Eq,
        Lit<int32_t>(std::numeric_limits<int32_t>::max(), kI32));
    Exec::SelectionVector secondOutput;
    eqMax.EvaluateSelection(batch, secondOutput);

    EXPECT_EQ(AsVec(secondOutput), (std::vector<Exec::RowId>{3}));
}

TEST_F(FixtureComparison, DateColumnComparedAgainstInt32Literal) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"d", kDate}}),
        MakeColumn<int32_t>({100, 200, 300})));

    Exec::ComparisonExpression expr(
        Col("d", kDate), Exec::CompareOp::Gte, Lit<int32_t>(200, kI32));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{1, 2}));
}

TEST_F(FixtureComparison, TimestampColumnComparedAgainstInt64Literal) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"ts", kTs}}),
        MakeColumn<int64_t>({1000, 2000, 3000})));

    Exec::ComparisonExpression expr(
        Col("ts", kTs), Exec::CompareOp::Lt, Lit<int64_t>(2500, kI64));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 1}));
}

TEST_F(FixtureComparison, BoolColumnComparedAgainstLiteralTrue) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"flag", kBool}}),
        MakeColumn<uint8_t>({1, 0, 1, 0, 1})));

    Exec::ComparisonExpression expr(
        Col("flag", kBool), Exec::CompareOp::Eq, Lit<uint8_t>(1, kBool));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 2, 4}));
}

TEST_F(FixtureComparison, Int128ColumnColumnComparesFullWidth) {
    const Int128 big = Int128(int64_t{1}) << 100;
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"a", kI128}, {"b", kI128}}),
        MakeColumn<Int128>({big, big - Int128(int64_t{1}), big + Int128(int64_t{2})}),
        MakeColumn<Int128>({big, big, big})));

    Exec::ComparisonExpression expr(Col("a", kI128), Exec::CompareOp::Lt, Col("b", kI128));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{1}));
}

TEST_F(FixtureComparison, Int128ColumnLiteralComparesFullWidth) {
    const Int128 big = Int128(int64_t{1}) << 100;
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI128}}),
        MakeColumn<Int128>({Int128(int64_t{0}), big - Int128(int64_t{1}), big, big + Int128(int64_t{1})})));

    Exec::ComparisonExpression expr(
        Col("x", kI128), Exec::CompareOp::Gte, Lit<Int128>(big, kI128));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{2, 3}));
}

TEST_F(FixtureComparison, EvaluateSelectionClearsOutputBeforeFilling) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3})));
    output_.Push(99);
    output_.Push(100);

    Exec::ComparisonExpression expr(Col("x", kI64), Exec::CompareOp::Eq, Lit<int64_t>(2, kI64));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{1}))
        << "stale rows from previous use must not leak into the result";
}

TEST_F(FixtureComparison, RespectsInputSelectionVector) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({10, 20, 30, 40, 50}));
    auto batch = MakeBatchWithSelection(rowGroup, {1, 3});

    Exec::ComparisonExpression expr(
        Col("x", kI64), Exec::CompareOp::Gt, Lit<int64_t>(15, kI64));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{1, 3}))
        << "rows outside the input selection are invisible even if they would pass";
}

TEST_F(FixtureComparison, OutputPreservesInputSelectionOrder) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({10, 20, 30, 40, 50}));
    auto batch = MakeBatchWithSelection(rowGroup, {4, 2, 0});

    Exec::ComparisonExpression expr(
        Col("x", kI64), Exec::CompareOp::Gt, Lit<int64_t>(5, kI64));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{4, 2, 0}))
        << "passing rows keep the order they had in the input selection";
}

TEST_F(FixtureComparison, EmptyBatchProducesEmptyOutput) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({})));

    Exec::ComparisonExpression expr(Col("x", kI64), Exec::CompareOp::Eq, Lit<int64_t>(0, kI64));
    expr.EvaluateSelection(batch, output_);

    EXPECT_TRUE(output_.Empty());
}

TEST_F(FixtureComparison, AllRowsPassPredicate) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 1, 1})));

    Exec::ComparisonExpression expr(Col("x", kI64), Exec::CompareOp::Eq, Lit<int64_t>(1, kI64));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 1, 2}));
}

TEST_F(FixtureComparison, NoRowsPassPredicate) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3})));

    Exec::ComparisonExpression expr(Col("x", kI64), Exec::CompareOp::Gt, Lit<int64_t>(100, kI64));
    expr.EvaluateSelection(batch, output_);

    EXPECT_TRUE(output_.Empty());
}

TEST_F(FixtureComparison, LiteralVsLiteralShapeIsRejectedAtEvaluation) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1})));

    Exec::ComparisonExpression expr(
        Lit<int64_t>(1, kI64), Exec::CompareOp::Eq, Lit<int64_t>(2, kI64));

    EXPECT_THROW(expr.EvaluateSelection(batch, output_), std::runtime_error);
}

TEST_F(FixtureComparison, ColumnVsArithmeticShapeIsRejectedAtEvaluation) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}, {"y", kI64}}),
        MakeColumn<int64_t>({1, 2}),
        MakeColumn<int64_t>({3, 4})));

    auto rhs = std::make_unique<Exec::ArithmeticExpression>(
        Col("x", kI64), Exec::ArithmOp::Add, Lit<int64_t>(1, kI64));

    Exec::ComparisonExpression expr(Col("y", kI64), Exec::CompareOp::Eq, std::move(rhs));

    EXPECT_THROW(expr.EvaluateSelection(batch, output_), std::runtime_error);
}

}  // namespace Columnar::Test
