#include <exec/expression/column_ref/column_ref.h>

#include <tests/lib/data_builders.h>
#include <tests/lib/exec_batch_builders.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <variant>

namespace Columnar::Test {

namespace {

constexpr auto kI16 = Types::LogicalType::INT16;
constexpr auto kI32 = Types::LogicalType::INT32;
constexpr auto kI64 = Types::LogicalType::INT64;
constexpr auto kStr = Types::LogicalType::STRING;
constexpr auto kBool = Types::LogicalType::BOOL;

class FixtureColumnRef : public ::testing::Test {
protected:
    void SetUp() override {
        rowGroup_ = MakeRowGroupOf(
            MakeSchema({{"id", kI64}, {"name", kStr}}),
            MakeColumn<int64_t>({100, 200, 300, 400}),
            MakeColumn<std::string>({"alice", "bob", "carol", "dave"}));
        batch_ = MakeBatch(rowGroup_);
    }

    std::shared_ptr<RowGroup> rowGroup_;
    Exec::ExecBatch batch_;
    Exec::EvalState state_;
};

template <typename T>
std::span<const T> AsSpan(const Exec::ColumnSpan& span) {
    return std::get<std::span<const T>>(span);
}

}  // namespace

TEST_F(FixtureColumnRef, KindReportsColumnRef) {
    Exec::ColumnRefExpression expr("id", kI64);

    EXPECT_EQ(expr.Kind(), Exec::ExpressionKind::ColumnRef);
}

TEST_F(FixtureColumnRef, ResultTypeMatchesConstructorArgument) {
    Exec::ColumnRefExpression expr("id", kI64);

    EXPECT_EQ(expr.ResultType(), kI64);
}

TEST_F(FixtureColumnRef, NameReturnsReferencedColumn) {
    Exec::ColumnRefExpression expr("payload", kStr);

    EXPECT_EQ(expr.Name(), "payload");
}

TEST_F(FixtureColumnRef, RequiredColumnsListsOnlyReferencedColumn) {
    Exec::ColumnRefExpression expr("id", kI64);

    const auto cols = expr.RequiredColumns();

    ASSERT_EQ(cols.size(), 1u);
    EXPECT_EQ(cols[0], "id");
}

TEST_F(FixtureColumnRef, EvaluateColumnReturnsAllRowsWithoutSelection) {
    Exec::ColumnRefExpression expr("id", kI64);

    const auto result = AsSpan<int64_t>(expr.EvaluateColumn(batch_, state_));

    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], 100);
    EXPECT_EQ(result[1], 200);
    EXPECT_EQ(result[2], 300);
    EXPECT_EQ(result[3], 400);
}

TEST_F(FixtureColumnRef, EvaluateColumnWithoutSelectionIsZeroCopy) {
    Exec::ColumnRefExpression expr("id", kI64);

    const int64_t* sourcePtr = rowGroup_->GetColumn(0).GetTypedData<int64_t>().data();
    const auto result = AsSpan<int64_t>(expr.EvaluateColumn(batch_, state_));

    EXPECT_EQ(result.data(), sourcePtr)
        << "without selection the span must point into the RowGroup's own buffer";
}

TEST_F(FixtureColumnRef, EvaluateColumnWithSelectionGathersSelectedRows) {
    auto selected = MakeBatchWithSelection(rowGroup_, {2, 0, 3});
    Exec::ColumnRefExpression expr("id", kI64);

    const auto result = AsSpan<int64_t>(expr.EvaluateColumn(selected, state_));

    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], 300);
    EXPECT_EQ(result[1], 100);
    EXPECT_EQ(result[2], 400);
}

TEST_F(FixtureColumnRef, EvaluateColumnWithSelectionWritesIntoEvalStateBuffer) {
    auto selected = MakeBatchWithSelection(rowGroup_, {1, 2});
    Exec::ColumnRefExpression expr("id", kI64);

    const int64_t* sourcePtr = rowGroup_->GetColumn(0).GetTypedData<int64_t>().data();
    const auto result = AsSpan<int64_t>(expr.EvaluateColumn(selected, state_));

    EXPECT_NE(result.data(), sourcePtr)
        << "with selection the span must point into state buffer, not the source";
}

TEST_F(FixtureColumnRef, EvaluateColumnWithEmptySelectionReturnsEmptySpan) {
    auto selected = MakeBatchWithSelection(rowGroup_, {});
    Exec::ColumnRefExpression expr("id", kI64);

    const auto result = AsSpan<int64_t>(expr.EvaluateColumn(selected, state_));

    EXPECT_TRUE(result.empty());
}

TEST_F(FixtureColumnRef, EvaluateColumnHandlesStringColumns) {
    Exec::ColumnRefExpression expr("name", kStr);

    const auto result = AsSpan<std::string>(expr.EvaluateColumn(batch_, state_));

    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], "alice");
    EXPECT_EQ(result[3], "dave");
}

TEST_F(FixtureColumnRef, EvaluateColumnHandlesInt16Columns) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"width", kI16}}),
        MakeColumn<int16_t>({640, 800, 1024}));
    auto batch = MakeBatch(rowGroup);

    Exec::ColumnRefExpression expr("width", kI16);
    Exec::EvalState state;

    const auto result = AsSpan<int16_t>(expr.EvaluateColumn(batch, state));

    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], 640);
    EXPECT_EQ(result[2], 1024);
}

TEST_F(FixtureColumnRef, EvaluateColumnHandlesBoolColumns) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"flag", kBool}}),
        MakeColumn<uint8_t>({1, 0, 1}));
    auto batch = MakeBatch(rowGroup);

    Exec::ColumnRefExpression expr("flag", kBool);
    Exec::EvalState state;

    const auto result = AsSpan<uint8_t>(expr.EvaluateColumn(batch, state));

    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 0);
    EXPECT_EQ(result[2], 1);
}

TEST_F(FixtureColumnRef, EvaluateScalarReturnsValueAtGivenRow) {
    Exec::ColumnRefExpression expr("id", kI64);

    const auto value = std::get<int64_t>(expr.EvaluateScalar(batch_, /*row=*/2));

    EXPECT_EQ(value, 300);
}

TEST_F(FixtureColumnRef, EvaluateScalarIgnoresSelectionVector) {
    auto selected = MakeBatchWithSelection(rowGroup_, {3});
    Exec::ColumnRefExpression expr("id", kI64);

    const auto value = std::get<int64_t>(expr.EvaluateScalar(selected, /*row=*/0));

    EXPECT_EQ(value, 100)
        << "EvaluateScalar takes a physical row index, selection is bypassed";
}

TEST_F(FixtureColumnRef, EvaluateScalarReturnsStringForStringColumn) {
    Exec::ColumnRefExpression expr("name", kStr);

    const auto value = std::get<std::string>(expr.EvaluateScalar(batch_, /*row=*/1));

    EXPECT_EQ(value, "bob");
}

TEST_F(FixtureColumnRef, EvaluateScalarReturnsInt32ForInt32Column) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"counter", kI32}}),
        MakeColumn<int32_t>({7, 42, 99}));
    auto batch = MakeBatch(rowGroup);

    Exec::ColumnRefExpression expr("counter", kI32);

    const auto value = std::get<int32_t>(expr.EvaluateScalar(batch, /*row=*/1));

    EXPECT_EQ(value, 42);
}

}  // namespace Columnar::Test
