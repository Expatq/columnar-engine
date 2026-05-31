#include <exec/expression/not/not.h>

#include <exec/expression/column_ref/column_ref.h>
#include <exec/expression/comparison/comparison.h>
#include <exec/expression/literal/literal.h>
#include <exec/expression/logical/logical.h>

#include <exec/core/exec_batch.h>
#include <exec/core/selection_vector.h>
#include <exec/interface/expression.h>

#include <tests/lib/data_builders.h>
#include <tests/lib/exec_batch_builders.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Columnar::Test {

namespace {

constexpr auto kI32 = Types::LogicalType::INT32;
constexpr auto kI64 = Types::LogicalType::INT64;
constexpr auto kStr = Types::LogicalType::STRING;
constexpr auto kBool = Types::LogicalType::BOOL;

std::unique_ptr<Exec::IExpression> Col(std::string name, Types::LogicalType type) {
    return std::make_unique<Exec::ColumnRefExpression>(std::move(name), type);
}

template <typename T>
std::unique_ptr<Exec::IExpression> Lit(T value, Types::LogicalType type) {
    return std::make_unique<Exec::LiteralExpression>(Types::AnyPhysicalType{value}, type);
}

std::unique_ptr<Exec::IExpression> Cmp(std::string col,
                                       Exec::CompareOp op,
                                       int64_t value) {
    return std::make_unique<Exec::ComparisonExpression>(
        Col(std::move(col), kI64), op, Lit<int64_t>(value, kI64));
}

std::unique_ptr<Exec::IExpression> Not(std::unique_ptr<Exec::IExpression> inner) {
    return std::make_unique<Exec::NotExpression>(std::move(inner));
}

template <typename... Args>
std::vector<std::unique_ptr<Exec::IExpression>> OpVec(Args&&... args) {
    std::vector<std::unique_ptr<Exec::IExpression>> v;
    v.reserve(sizeof...(args));
    (v.emplace_back(std::forward<Args>(args)), ...);
    return v;
}

std::vector<Exec::RowId> AsVec(const Exec::SelectionVector& sel) {
    return {sel.Rows().begin(), sel.Rows().end()};
}

std::vector<Exec::RowId> Evaluate(Exec::IExpression& expr, const Exec::ExecBatch& batch) {
    Exec::SelectionVector sv;
    expr.EvaluateSelection(batch, sv);
    return AsVec(sv);
}

class FixtureNot : public ::testing::Test {
protected:
    Exec::SelectionVector output_;
};

}  // namespace

TEST_F(FixtureNot, KindReportsNot) {
    Exec::NotExpression expr(Cmp("x", Exec::CompareOp::Eq, 1));

    EXPECT_EQ(expr.Kind(), Exec::ExpressionKind::Not);
}

TEST_F(FixtureNot, ResultTypeIsBool) {
    Exec::NotExpression expr(Cmp("x", Exec::CompareOp::Eq, 1));

    EXPECT_EQ(expr.ResultType(), kBool);
}

TEST_F(FixtureNot, RequiredColumnsForwardsFromOperand) {
    Exec::NotExpression expr(std::make_unique<Exec::ComparisonExpression>(
        Col("a", kI64), Exec::CompareOp::Eq, Col("b", kI64)));

    auto cols = expr.RequiredColumns();
    std::sort(cols.begin(), cols.end());

    ASSERT_EQ(cols.size(), 2u);
    EXPECT_EQ(cols[0], "a");
    EXPECT_EQ(cols[1], "b");
}

TEST_F(FixtureNot, ConstructorRejectsNullOperand) {
    EXPECT_THROW(Exec::NotExpression(nullptr), std::invalid_argument);
}

TEST_F(FixtureNot, ConstructorRejectsNonBoolOperand) {
    EXPECT_THROW(
        Exec::NotExpression(Col("x", kI64)),
        std::invalid_argument)
        << "ColumnRef of INT64 has ResultType()==INT64, not BOOL";
}

TEST_F(FixtureNot, NegatesPredicateMatchingSomeRows) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 5, 10, 15, 20})));

    Exec::NotExpression expr(Cmp("x", Exec::CompareOp::Gt, 10));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 1, 2}));
}

TEST_F(FixtureNot, NegatesPredicateMatchingNoRowsReturnsAllRows) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3})));

    Exec::NotExpression expr(Cmp("x", Exec::CompareOp::Gt, 100));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 1, 2}));
}

TEST_F(FixtureNot, NegatesPredicateMatchingAllRowsReturnsEmpty) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3})));

    Exec::NotExpression expr(Cmp("x", Exec::CompareOp::Lt, 100));
    expr.EvaluateSelection(batch, output_);

    EXPECT_TRUE(output_.Empty());
}

TEST_F(FixtureNot, RespectsInputSelectionVector) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({10, 20, 30, 40, 50}));
    auto batch = MakeBatchWithSelection(rowGroup, {1, 3});

    Exec::NotExpression expr(Cmp("x", Exec::CompareOp::Eq, 20));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{3}))
        << "rows outside input selection (0, 2, 4) must never appear in output";
}

TEST_F(FixtureNot, PreservesInputSelectionOrder) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({10, 20, 30, 40, 50}));
    auto batch = MakeBatchWithSelection(rowGroup, {4, 1, 3, 2});

    Exec::NotExpression expr(Cmp("x", Exec::CompareOp::Eq, 30));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{4, 1, 3}))
        << "surviving rows keep their position in the input selection";
}

TEST_F(FixtureNot, EmptyBatchReturnsEmpty) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({})));

    Exec::NotExpression expr(Cmp("x", Exec::CompareOp::Eq, 0));
    expr.EvaluateSelection(batch, output_);

    EXPECT_TRUE(output_.Empty());
}

TEST_F(FixtureNot, ClearsOutputBeforePopulating) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3})));
    output_.Push(99);
    output_.Push(100);

    Exec::NotExpression expr(Cmp("x", Exec::CompareOp::Eq, 2));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 2}))
        << "stale rows from previous use must not leak into the result";
}

TEST_F(FixtureNot, DoubleNegationReturnsOriginalRows) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 5, 10, 15, 20})));

    Exec::NotExpression expr(Not(Cmp("x", Exec::CompareOp::Gt, 10)));

    EXPECT_EQ(Evaluate(expr, batch), (std::vector<Exec::RowId>{3, 4}));
}

TEST_F(FixtureNot, AndOfPredicateAndItsNegationIsAlwaysEmpty) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3, 4, 5})));

    Exec::LogicalExpression expr(
        Exec::LogicalOp::And,
        OpVec(Cmp("x", Exec::CompareOp::Gt, 2),
              Not(Cmp("x", Exec::CompareOp::Gt, 2))));

    EXPECT_TRUE(Evaluate(expr, batch).empty());
}

TEST_F(FixtureNot, OrOfPredicateAndItsNegationCoversEveryRow) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3, 4, 5})));

    Exec::LogicalExpression expr(
        Exec::LogicalOp::Or,
        OpVec(Cmp("x", Exec::CompareOp::Gt, 2),
              Not(Cmp("x", Exec::CompareOp::Gt, 2))));

    EXPECT_EQ(Evaluate(expr, batch), (std::vector<Exec::RowId>{0, 1, 2, 3, 4}));
}

TEST_F(FixtureNot, DeMorganNotAndEqualsOrOfNots) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 5, 10, 15, 20, 25})));

    Exec::NotExpression notOfAnd(std::make_unique<Exec::LogicalExpression>(
        Exec::LogicalOp::And,
        OpVec(Cmp("x", Exec::CompareOp::Gt, 5),
              Cmp("x", Exec::CompareOp::Lt, 20))));

    Exec::LogicalExpression orOfNots(
        Exec::LogicalOp::Or,
        OpVec(Not(Cmp("x", Exec::CompareOp::Gt, 5)),
              Not(Cmp("x", Exec::CompareOp::Lt, 20))));

    EXPECT_EQ(Evaluate(notOfAnd, batch), Evaluate(orOfNots, batch));
}

TEST_F(FixtureNot, DeMorganNotOrEqualsAndOfNots) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 5, 10, 15, 20, 25})));

    Exec::NotExpression notOfOr(std::make_unique<Exec::LogicalExpression>(
        Exec::LogicalOp::Or,
        OpVec(Cmp("x", Exec::CompareOp::Lt, 5),
              Cmp("x", Exec::CompareOp::Gt, 20))));

    Exec::LogicalExpression andOfNots(
        Exec::LogicalOp::And,
        OpVec(Not(Cmp("x", Exec::CompareOp::Lt, 5)),
              Not(Cmp("x", Exec::CompareOp::Gt, 20))));

    EXPECT_EQ(Evaluate(notOfOr, batch), Evaluate(andOfNots, batch));
}

TEST_F(FixtureNot, NegationOfStringEqualitySelectsNonMatchingRows) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"name", kStr}}),
        MakeColumn<std::string>({"alice", "bob", "alice", "carol"})));

    Exec::NotExpression expr(std::make_unique<Exec::ComparisonExpression>(
        Col("name", kStr),
        Exec::CompareOp::Eq,
        Lit<std::string>(std::string{"alice"}, kStr)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{1, 3}));
}

TEST_F(FixtureNot, NegationOnNegativeIntegersCovary) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI32}}),
        MakeColumn<int32_t>({-100, -1, 0, 1, 100})));

    Exec::NotExpression expr(std::make_unique<Exec::ComparisonExpression>(
        Col("x", kI32),
        Exec::CompareOp::Lt,
        Lit<int32_t>(0, kI32)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{2, 3, 4}));
}

}  // namespace Columnar::Test
