#include <exec/expression/logical/logical.h>

#include <exec/expression/column_ref/column_ref.h>
#include <exec/expression/comparison/comparison.h>
#include <exec/expression/literal/literal.h>

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

std::unique_ptr<Exec::IExpression> CmpEq(std::string col, int64_t value) {
    return std::make_unique<Exec::ComparisonExpression>(
        Col(std::move(col), kI64), Exec::CompareOp::Eq, Lit<int64_t>(value, kI64));
}

std::unique_ptr<Exec::IExpression> CmpGt(std::string col, int64_t value) {
    return std::make_unique<Exec::ComparisonExpression>(
        Col(std::move(col), kI64), Exec::CompareOp::Gt, Lit<int64_t>(value, kI64));
}

std::unique_ptr<Exec::IExpression> CmpLt(std::string col, int64_t value) {
    return std::make_unique<Exec::ComparisonExpression>(
        Col(std::move(col), kI64), Exec::CompareOp::Lt, Lit<int64_t>(value, kI64));
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

class CountingPredicate : public Exec::IExpression {
public:
    CountingPredicate(bool matchAll, int* counter)
        : matchAll_(matchAll),
          counter_(counter) {
    }

    Exec::ExpressionKind Kind() const override {
        return Exec::ExpressionKind::Comparison;
    }

    Types::LogicalType ResultType() const override {
        return Types::LogicalType::BOOL;
    }

    std::vector<std::string> RequiredColumns() const override {
        return {};
    }

    void EvaluateSelection(const Exec::ExecBatch& input, Exec::SelectionVector& out) const override {
        ++*counter_;
        out.Clear();
        if (!matchAll_) {
            return;
        }
        if (input.has_selection) {
            for (auto row : input.selection.Rows()) {
                out.Push(row);
            }
            return;
        }
        for (Exec::RowId row = 0; row < input.rowCount; ++row) {
            out.Push(row);
        }
    }

private:
    bool matchAll_;
    int* counter_;
};

std::unique_ptr<Exec::IExpression> Probe(bool matchAll, int* counter) {
    return std::make_unique<CountingPredicate>(matchAll, counter);
}

class FixtureLogical : public ::testing::Test {
protected:
    Exec::SelectionVector output_;
};

}  // namespace

TEST_F(FixtureLogical, KindReportsLogical) {
    Exec::LogicalExpression expr(Exec::LogicalOp::And, OpVec(CmpEq("x", 1), CmpEq("x", 1)));

    EXPECT_EQ(expr.Kind(), Exec::ExpressionKind::Logical);
}

TEST_F(FixtureLogical, ResultTypeIsBool) {
    Exec::LogicalExpression expr(Exec::LogicalOp::Or, OpVec(CmpEq("x", 1), CmpEq("x", 2)));

    EXPECT_EQ(expr.ResultType(), kBool);
}

TEST_F(FixtureLogical, RequiredColumnsUnionsOperandColumns) {
    Exec::LogicalExpression expr(
        Exec::LogicalOp::And,
        OpVec(CmpEq("a", 1), CmpEq("b", 2), CmpEq("c", 3)));

    auto cols = expr.RequiredColumns();
    std::sort(cols.begin(), cols.end());

    ASSERT_EQ(cols.size(), 3u);
    EXPECT_EQ(cols[0], "a");
    EXPECT_EQ(cols[1], "b");
    EXPECT_EQ(cols[2], "c");
}

TEST_F(FixtureLogical, RequiredColumnsDeduplicatesAcrossOperands) {
    Exec::LogicalExpression expr(
        Exec::LogicalOp::Or,
        OpVec(CmpEq("a", 1), CmpEq("a", 2), CmpEq("b", 3)));

    auto cols = expr.RequiredColumns();
    std::sort(cols.begin(), cols.end());

    ASSERT_EQ(cols.size(), 2u);
    EXPECT_EQ(cols[0], "a");
    EXPECT_EQ(cols[1], "b");
}

TEST_F(FixtureLogical, ConstructorRejectsEmptyOperandList) {
    std::vector<std::unique_ptr<Exec::IExpression>> empty;

    EXPECT_THROW(
        Exec::LogicalExpression(Exec::LogicalOp::And, std::move(empty)),
        std::invalid_argument);
}

TEST_F(FixtureLogical, ConstructorRejectsSingleOperand) {
    EXPECT_THROW(
        Exec::LogicalExpression(Exec::LogicalOp::And, OpVec(CmpEq("x", 1))),
        std::invalid_argument);
}

TEST_F(FixtureLogical, ConstructorRejectsNonBoolOperand) {
    EXPECT_THROW(
        Exec::LogicalExpression(Exec::LogicalOp::And, OpVec(CmpEq("x", 1), Col("y", kI64))),
        std::invalid_argument)
        << "ColumnRef of INT64 has ResultType()==INT64, not BOOL";
}

TEST_F(FixtureLogical, AndShortCircuitsAfterFirstOperandReturnsEmpty) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3})));

    int firstCalls = 0;
    int secondCalls = 0;
    int thirdCalls = 0;

    Exec::LogicalExpression expr(
        Exec::LogicalOp::And,
        OpVec(Probe(/*matchAll=*/false, &firstCalls),
              Probe(true, &secondCalls),
              Probe(true, &thirdCalls)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(firstCalls, 1);
    EXPECT_EQ(secondCalls, 0) << "first operand killed all rows, second must be skipped";
    EXPECT_EQ(thirdCalls, 0);
    EXPECT_TRUE(output_.Empty());
}

TEST_F(FixtureLogical, AndEvaluatesLaterOperandsWhileRowsSurvive) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3})));

    int firstCalls = 0;
    int secondCalls = 0;

    Exec::LogicalExpression expr(
        Exec::LogicalOp::And,
        OpVec(Probe(/*matchAll=*/true, &firstCalls),
              Probe(true, &secondCalls)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(firstCalls, 1);
    EXPECT_EQ(secondCalls, 1);
    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 1, 2}));
}

TEST_F(FixtureLogical, AndOfTwoSelectsIntersection) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 5, 10, 15, 20, 25})));

    Exec::LogicalExpression expr(Exec::LogicalOp::And, OpVec(CmpGt("x", 5), CmpLt("x", 25)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{2, 3, 4}));
}

TEST_F(FixtureLogical, AndOfFourPredicatesIntersectsAll) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 5, 10, 15, 20, 25, 30})));

    Exec::LogicalExpression expr(
        Exec::LogicalOp::And,
        OpVec(CmpGt("x", 5), CmpLt("x", 30), CmpGt("x", 10), CmpLt("x", 25)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{3, 4}));
}

TEST_F(FixtureLogical, AndWithImpossiblePredicateReturnsEmpty) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3})));

    Exec::LogicalExpression expr(
        Exec::LogicalOp::And, OpVec(CmpLt("x", 0), CmpGt("x", 0)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_TRUE(output_.Empty());
}

TEST_F(FixtureLogical, AndWithAllMatchingPredicatesReturnsAllRows) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({10, 20, 30})));

    Exec::LogicalExpression expr(
        Exec::LogicalOp::And, OpVec(CmpGt("x", 0), CmpLt("x", 100)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 1, 2}));
}

TEST_F(FixtureLogical, OrOfTwoSelectsUnion) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 5, 10, 15, 20})));

    Exec::LogicalExpression expr(
        Exec::LogicalOp::Or, OpVec(CmpLt("x", 5), CmpGt("x", 15)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 4}));
}

TEST_F(FixtureLogical, OrOfThreeAccumulatesAcrossOperands) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3, 4, 5})));

    Exec::LogicalExpression expr(
        Exec::LogicalOp::Or,
        OpVec(CmpEq("x", 1), CmpEq("x", 3), CmpEq("x", 5)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 2, 4}));
}

TEST_F(FixtureLogical, OrDeduplicatesOverlappingMatches) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3, 4, 5})));

    Exec::LogicalExpression expr(
        Exec::LogicalOp::Or, OpVec(CmpLt("x", 4), CmpGt("x", 2)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 1, 2, 3, 4}))
        << "row 2 (value=3) matches both predicates but appears only once";
}

TEST_F(FixtureLogical, OrShortCircuitsAfterAllRowsMatched) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3})));

    int firstCalls = 0;
    int secondCalls = 0;
    int thirdCalls = 0;

    Exec::LogicalExpression expr(
        Exec::LogicalOp::Or,
        OpVec(Probe(/*matchAll=*/true, &firstCalls),
              Probe(false, &secondCalls),
              Probe(false, &thirdCalls)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(firstCalls, 1);
    EXPECT_EQ(secondCalls, 0) << "remaining became empty, later operands must be skipped";
    EXPECT_EQ(thirdCalls, 0);
    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 1, 2}));
}

TEST_F(FixtureLogical, OrEvaluatesLaterOperandsWhileRowsRemain) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3})));

    int firstCalls = 0;
    int secondCalls = 0;

    Exec::LogicalExpression expr(
        Exec::LogicalOp::Or,
        OpVec(Probe(/*matchAll=*/false, &firstCalls),
              Probe(true, &secondCalls)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(firstCalls, 1);
    EXPECT_EQ(secondCalls, 1) << "no rows matched yet, second operand must run";
    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 1, 2}));
}

TEST_F(FixtureLogical, AndPreservesInputSelectionOrder) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({10, 20, 30, 40, 50}));
    auto batch = MakeBatchWithSelection(rowGroup, {3, 1, 4});

    Exec::LogicalExpression expr(
        Exec::LogicalOp::And, OpVec(CmpGt("x", 0), CmpLt("x", 100)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{3, 1, 4}));
}

TEST_F(FixtureLogical, OrPreservesInputSelectionOrder) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({10, 20, 30, 40, 50}));
    auto batch = MakeBatchWithSelection(rowGroup, {3, 1, 4});

    Exec::LogicalExpression expr(
        Exec::LogicalOp::Or, OpVec(CmpLt("x", 25), CmpGt("x", 45)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{1, 4}));
}

TEST_F(FixtureLogical, AndIgnoresRowsOutsideInputSelection) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({10, 20, 30, 40, 50}));
    auto batch = MakeBatchWithSelection(rowGroup, {1, 3});

    Exec::LogicalExpression expr(
        Exec::LogicalOp::And, OpVec(CmpGt("x", 5), CmpLt("x", 100)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{1, 3}))
        << "rows 0,2,4 satisfy predicates but are outside input selection";
}

TEST_F(FixtureLogical, OrIgnoresRowsOutsideInputSelection) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({10, 20, 30, 40, 50}));
    auto batch = MakeBatchWithSelection(rowGroup, {1, 3});

    Exec::LogicalExpression expr(
        Exec::LogicalOp::Or, OpVec(CmpEq("x", 10), CmpEq("x", 40)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{3}))
        << "row 0 (value=10) is outside the input selection and must be invisible";
}

TEST_F(FixtureLogical, AndOnEmptyBatchReturnsEmpty) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({})));

    Exec::LogicalExpression expr(
        Exec::LogicalOp::And, OpVec(CmpGt("x", 0), CmpLt("x", 100)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_TRUE(output_.Empty());
}

TEST_F(FixtureLogical, OrOnEmptyBatchReturnsEmpty) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({})));

    Exec::LogicalExpression expr(
        Exec::LogicalOp::Or, OpVec(CmpEq("x", 1), CmpEq("x", 2)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_TRUE(output_.Empty());
}

TEST_F(FixtureLogical, AndOfOrEvaluatesNestedCorrectly) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 5, 10, 50, 100, 500})));

    auto orExpr = std::make_unique<Exec::LogicalExpression>(
        Exec::LogicalOp::Or, OpVec(CmpEq("x", 10), CmpEq("x", 100)));
    Exec::LogicalExpression expr(
        Exec::LogicalOp::And, OpVec(CmpGt("x", 1), std::move(orExpr)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{2, 4}));
}

TEST_F(FixtureLogical, OrOfAndEvaluatesNestedCorrectly) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 5, 10, 50, 100})));

    auto andOne = std::make_unique<Exec::LogicalExpression>(
        Exec::LogicalOp::And, OpVec(CmpGt("x", 0), CmpLt("x", 10)));
    auto andTwo = std::make_unique<Exec::LogicalExpression>(
        Exec::LogicalOp::And, OpVec(CmpGt("x", 50), CmpLt("x", 200)));
    Exec::LogicalExpression expr(
        Exec::LogicalOp::Or, OpVec(std::move(andOne), std::move(andTwo)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 1, 4}));
}

TEST_F(FixtureLogical, OrClearsOutputBeforePopulating) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3, 4, 5})));
    output_.Push(99);
    output_.Push(100);

    Exec::LogicalExpression expr(
        Exec::LogicalOp::Or, OpVec(CmpEq("x", 1), CmpEq("x", 5)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 4}))
        << "stale rows from previous use must not leak into the result";
}

TEST_F(FixtureLogical, AndClearsOutputBeforePopulating) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3, 4, 5})));
    output_.Push(99);
    output_.Push(100);

    Exec::LogicalExpression expr(
        Exec::LogicalOp::And, OpVec(CmpGt("x", 1), CmpLt("x", 5)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{1, 2, 3}))
        << "first operand of AND must reset out via its own EvaluateSelection";
}

TEST_F(FixtureLogical, AndOnStringPredicates) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"name", kStr}, {"city", kStr}}),
        MakeColumn<std::string>({"alice", "bob", "carol", "alice"}),
        MakeColumn<std::string>({"moscow", "moscow", "kazan", "kazan"})));

    auto eqAlice = std::make_unique<Exec::ComparisonExpression>(
        Col("name", kStr), Exec::CompareOp::Eq, Lit<std::string>(std::string{"alice"}, kStr));
    auto eqMoscow = std::make_unique<Exec::ComparisonExpression>(
        Col("city", kStr), Exec::CompareOp::Eq, Lit<std::string>(std::string{"moscow"}, kStr));

    Exec::LogicalExpression expr(
        Exec::LogicalOp::And, OpVec(std::move(eqAlice), std::move(eqMoscow)));
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0}));
}

}  // namespace Columnar::Test
