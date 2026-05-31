#include <exec/expression/arithmetic/arithmetic.h>
#include <exec/expression/column_ref/column_ref.h>
#include <exec/expression/literal/literal.h>

#include <tests/lib/data_builders.h>
#include <tests/lib/exec_batch_builders.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <variant>

namespace Columnar::Test {

namespace {

constexpr auto kI64 = Types::LogicalType::INT64;
constexpr auto kI16 = Types::LogicalType::INT16;
constexpr auto kStr = Types::LogicalType::STRING;

class FixtureArithmetic : public ::testing::Test {
protected:
    void SetUp() override {
        rowGroup_ = MakeRowGroupOf(
            MakeSchema({{"a", kI64}, {"b", kI64}}),
            MakeColumn<int64_t>({10, 20, 30, 40}),
            MakeColumn<int64_t>({1, 2, 3, 5}));
        batch_ = MakeBatch(rowGroup_);
    }

    static std::unique_ptr<Exec::IExpression> ColA() {
        return std::make_unique<Exec::ColumnRefExpression>("a", kI64);
    }

    static std::unique_ptr<Exec::IExpression> ColB() {
        return std::make_unique<Exec::ColumnRefExpression>("b", kI64);
    }

    static std::unique_ptr<Exec::IExpression> LitI64(int64_t value) {
        return std::make_unique<Exec::LiteralExpression>(value, kI64);
    }

    static std::span<const int64_t> AsInt64Span(const Exec::ColumnSpan& span) {
        return std::get<std::span<const int64_t>>(span);
    }

    std::shared_ptr<RowGroup> rowGroup_;
    Exec::ExecBatch batch_;
    Exec::EvalState state_;
};

}  // namespace

TEST_F(FixtureArithmetic, AddTwoColumnsElementWise) {
    Exec::ArithmeticExpression expr(ColA(), Exec::ArithmOp::Add, ColB());

    const auto result = AsInt64Span(expr.EvaluateColumn(batch_, state_));

    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], 11);
    EXPECT_EQ(result[1], 22);
    EXPECT_EQ(result[2], 33);
    EXPECT_EQ(result[3], 45);
}

TEST_F(FixtureArithmetic, SubtractRightFromLeft) {
    Exec::ArithmeticExpression expr(ColA(), Exec::ArithmOp::Sub, ColB());

    const auto result = AsInt64Span(expr.EvaluateColumn(batch_, state_));

    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], 9);
    EXPECT_EQ(result[1], 18);
    EXPECT_EQ(result[2], 27);
    EXPECT_EQ(result[3], 35);
}

TEST_F(FixtureArithmetic, MultiplyTwoColumns) {
    Exec::ArithmeticExpression expr(ColA(), Exec::ArithmOp::Mul, ColB());

    const auto result = AsInt64Span(expr.EvaluateColumn(batch_, state_));

    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], 10);
    EXPECT_EQ(result[1], 40);
    EXPECT_EQ(result[2], 90);
    EXPECT_EQ(result[3], 200);
}

TEST_F(FixtureArithmetic, DivideLeftByRight) {
    Exec::ArithmeticExpression expr(ColA(), Exec::ArithmOp::Div, ColB());

    const auto result = AsInt64Span(expr.EvaluateColumn(batch_, state_));

    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], 10);
    EXPECT_EQ(result[1], 10);
    EXPECT_EQ(result[2], 10);
    EXPECT_EQ(result[3], 8);
}

TEST_F(FixtureArithmetic, AddColumnAndLiteralBroadcasts) {
    Exec::ArithmeticExpression expr(ColA(), Exec::ArithmOp::Add, LitI64(100));

    const auto result = AsInt64Span(expr.EvaluateColumn(batch_, state_));

    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], 110);
    EXPECT_EQ(result[1], 120);
    EXPECT_EQ(result[2], 130);
    EXPECT_EQ(result[3], 140);
}

TEST_F(FixtureArithmetic, LiteralOnLeftAlsoBroadcasts) {
    Exec::ArithmeticExpression expr(LitI64(1000), Exec::ArithmOp::Sub, ColA());

    const auto result = AsInt64Span(expr.EvaluateColumn(batch_, state_));

    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], 990);
    EXPECT_EQ(result[1], 980);
    EXPECT_EQ(result[2], 970);
    EXPECT_EQ(result[3], 960);
}

TEST_F(FixtureArithmetic, ResultTypeIsAlwaysInt64) {
    Exec::ArithmeticExpression expr(ColA(), Exec::ArithmOp::Add, ColB());

    EXPECT_EQ(expr.ResultType(), Types::LogicalType::INT64);
}

TEST_F(FixtureArithmetic, KindReportsArithmetic) {
    Exec::ArithmeticExpression expr(ColA(), Exec::ArithmOp::Add, ColB());

    EXPECT_EQ(expr.Kind(), Exec::ExpressionKind::Arithmetic);
}

TEST_F(FixtureArithmetic, RequiredColumnsUnionsBothOperands) {
    Exec::ArithmeticExpression expr(ColA(), Exec::ArithmOp::Mul, ColB());

    auto cols = expr.RequiredColumns();
    std::sort(cols.begin(), cols.end());

    ASSERT_EQ(cols.size(), 2u);
    EXPECT_EQ(cols[0], "a");
    EXPECT_EQ(cols[1], "b");
}

TEST_F(FixtureArithmetic, RequiredColumnsDeduplicatesSharedColumn) {
    Exec::ArithmeticExpression expr(ColA(), Exec::ArithmOp::Add, ColA());

    auto cols = expr.RequiredColumns();

    ASSERT_EQ(cols.size(), 1u);
    EXPECT_EQ(cols[0], "a");
}

TEST_F(FixtureArithmetic, RequiredColumnsEmptyWhenBothLiterals) {
    Exec::ArithmeticExpression expr(LitI64(2), Exec::ArithmOp::Mul, LitI64(3));

    EXPECT_TRUE(expr.RequiredColumns().empty());
}

TEST_F(FixtureArithmetic, DivideByZeroThrows) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"x", kI64}, {"y", kI64}}),
        MakeColumn<int64_t>({10}),
        MakeColumn<int64_t>({0}));
    auto batch = MakeBatch(rowGroup);

    Exec::ArithmeticExpression expr(
        std::make_unique<Exec::ColumnRefExpression>("x", kI64),
        Exec::ArithmOp::Div,
        std::make_unique<Exec::ColumnRefExpression>("y", kI64));

    Exec::EvalState state;
    EXPECT_THROW(expr.EvaluateColumn(batch, state), std::runtime_error);
}

TEST_F(FixtureArithmetic, ConstructorRejectsNullLeftOperand) {
    EXPECT_THROW(
        Exec::ArithmeticExpression(nullptr, Exec::ArithmOp::Add, ColB()),
        std::invalid_argument);
}

TEST_F(FixtureArithmetic, ConstructorRejectsNullRightOperand) {
    EXPECT_THROW(
        Exec::ArithmeticExpression(ColA(), Exec::ArithmOp::Add, nullptr),
        std::invalid_argument);
}

TEST_F(FixtureArithmetic, EvaluateScalarComputesSingleRow) {
    Exec::ArithmeticExpression expr(ColA(), Exec::ArithmOp::Add, ColB());

    const auto value = std::get<int64_t>(expr.EvaluateScalar(batch_, /*row=*/2));

    EXPECT_EQ(value, 33);
}

TEST_F(FixtureArithmetic, EvaluateScalarOnStringOperandThrows) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"name", kStr}}),
        MakeColumn<std::string>({"foo"}));
    auto batch = MakeBatch(rowGroup);

    Exec::ArithmeticExpression expr(
        std::make_unique<Exec::ColumnRefExpression>("name", kStr),
        Exec::ArithmOp::Add,
        LitI64(1));

    EXPECT_THROW(expr.EvaluateScalar(batch, 0), std::runtime_error);
}

TEST_F(FixtureArithmetic, EvaluateColumnOnStringOperandThrows) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"name", kStr}}),
        MakeColumn<std::string>({"foo"}));
    auto batch = MakeBatch(rowGroup);

    Exec::ArithmeticExpression expr(
        std::make_unique<Exec::ColumnRefExpression>("name", kStr),
        Exec::ArithmOp::Add,
        LitI64(1));

    Exec::EvalState state;
    EXPECT_THROW(expr.EvaluateColumn(batch, state), std::runtime_error);
}

TEST_F(FixtureArithmetic, MixedIntegerWidthsPromoteToInt64) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"small", kI16}, {"big", kI64}}),
        MakeColumn<int16_t>({100, 200, 300}),
        MakeColumn<int64_t>({1, 2, 3}));
    auto batch = MakeBatch(rowGroup);

    Exec::ArithmeticExpression expr(
        std::make_unique<Exec::ColumnRefExpression>("small", kI16),
        Exec::ArithmOp::Mul,
        std::make_unique<Exec::ColumnRefExpression>("big", kI64));

    Exec::EvalState state;
    const auto result = AsInt64Span(expr.EvaluateColumn(batch, state));

    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], 100);
    EXPECT_EQ(result[1], 400);
    EXPECT_EQ(result[2], 900);
}

TEST_F(FixtureArithmetic, SelectionVectorFiltersInputBeforeArithmetic) {
    auto batchWithSelection = MakeBatchWithSelection(rowGroup_, {1, 3});

    Exec::ArithmeticExpression expr(ColA(), Exec::ArithmOp::Add, ColB());

    Exec::EvalState state;
    const auto result = AsInt64Span(expr.EvaluateColumn(batchWithSelection, state));

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], 22);
    EXPECT_EQ(result[1], 45);
}

}  // namespace Columnar::Test
