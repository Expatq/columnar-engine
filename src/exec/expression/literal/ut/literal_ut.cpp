#include <exec/expression/literal/literal.h>

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

class FixtureLiteral : public ::testing::Test {
protected:
    void SetUp() override {
        rowGroup_ = MakeRowGroupOf(
            MakeSchema({{"id", kI64}}),
            MakeColumn<int64_t>({1, 2, 3, 4, 5}));
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

TEST_F(FixtureLiteral, KindReportsLiteral) {
    Exec::LiteralExpression expr(Types::AnyPhysicalType{int64_t{42}}, kI64);

    EXPECT_EQ(expr.Kind(), Exec::ExpressionKind::Literal);
}

TEST_F(FixtureLiteral, ResultTypeMatchesConstructorArgument) {
    Exec::LiteralExpression expr(Types::AnyPhysicalType{int64_t{42}}, kI64);

    EXPECT_EQ(expr.ResultType(), kI64);
}

TEST_F(FixtureLiteral, RequiredColumnsIsEmpty) {
    Exec::LiteralExpression expr(Types::AnyPhysicalType{int64_t{42}}, kI64);

    EXPECT_TRUE(expr.RequiredColumns().empty());
}

TEST_F(FixtureLiteral, ValueReturnsConstructorArgument) {
    Exec::LiteralExpression expr(Types::AnyPhysicalType{int64_t{42}}, kI64);

    EXPECT_EQ(std::get<int64_t>(expr.Value()), 42);
}

TEST_F(FixtureLiteral, ConstructorRejectsValueWithWrongPhysicalType) {
    EXPECT_THROW(
        Exec::LiteralExpression(Types::AnyPhysicalType{int64_t{1}}, kStr),
        std::invalid_argument)
        << "int64 value cannot satisfy STRING logical type";
}

TEST_F(FixtureLiteral, ConstructorRejectsInt32ValueAgainstInt64Type) {
    EXPECT_THROW(
        Exec::LiteralExpression(Types::AnyPhysicalType{int32_t{1}}, kI64),
        std::invalid_argument)
        << "implicit int->int64 widening is a common footgun; constructor catches it";
}

TEST_F(FixtureLiteral, ConstructorAcceptsInt32ValueForDateLogicalType) {
    EXPECT_NO_THROW(
        Exec::LiteralExpression(Types::AnyPhysicalType{int32_t{1234}}, Types::LogicalType::DATE))
        << "DATE shares PhysicalType::INT32, so int32 value is valid";
}

TEST_F(FixtureLiteral, ConstructorAcceptsInt64ValueForTimestampLogicalType) {
    EXPECT_NO_THROW(
        Exec::LiteralExpression(Types::AnyPhysicalType{int64_t{1234567}}, Types::LogicalType::TIMESTAMP))
        << "TIMESTAMP shares PhysicalType::INT64";
}

TEST_F(FixtureLiteral, EvaluateColumnFillsAllActiveRowsWithLiteralValue) {
    Exec::LiteralExpression expr(Types::AnyPhysicalType{int64_t{7}}, kI64);

    const auto result = AsSpan<int64_t>(expr.EvaluateColumn(batch_, state_));

    ASSERT_EQ(result.size(), 5u);
    for (const auto value : result) {
        EXPECT_EQ(value, 7);
    }
}

TEST_F(FixtureLiteral, EvaluateColumnRespectsActiveRowCountUnderSelection) {
    auto selected = MakeBatchWithSelection(rowGroup_, {1, 3});
    Exec::LiteralExpression expr(Types::AnyPhysicalType{int64_t{9}}, kI64);

    const auto result = AsSpan<int64_t>(expr.EvaluateColumn(selected, state_));

    ASSERT_EQ(result.size(), 2u)
        << "buffer must be sized to ActiveRowCount, not raw rowCount";
    EXPECT_EQ(result[0], 9);
    EXPECT_EQ(result[1], 9);
}

TEST_F(FixtureLiteral, EvaluateColumnReturnsEmptyOnEmptyBatch) {
    auto emptyRowGroup = MakeRowGroupOf(
        MakeSchema({{"id", kI64}}),
        MakeColumn<int64_t>({}));
    auto emptyBatch = MakeBatch(emptyRowGroup);

    Exec::LiteralExpression expr(Types::AnyPhysicalType{int64_t{1}}, kI64);
    Exec::EvalState state;

    EXPECT_TRUE(AsSpan<int64_t>(expr.EvaluateColumn(emptyBatch, state)).empty());
}

TEST_F(FixtureLiteral, EvaluateColumnReusesEvalStateBufferAcrossCalls) {
    Exec::LiteralExpression expr(Types::AnyPhysicalType{int64_t{1}}, kI64);

    const int64_t* firstPtr = AsSpan<int64_t>(expr.EvaluateColumn(batch_, state_)).data();
    const int64_t* secondPtr = AsSpan<int64_t>(expr.EvaluateColumn(batch_, state_)).data();

    EXPECT_EQ(firstPtr, secondPtr)
        << "EvalState owns the buffer; repeated evaluations should not reallocate";
}

TEST_F(FixtureLiteral, EvaluateColumnHandlesStringLiteral) {
    Exec::LiteralExpression expr(Types::AnyPhysicalType{std::string{"hello"}}, kStr);

    const auto result = AsSpan<std::string>(expr.EvaluateColumn(batch_, state_));

    ASSERT_EQ(result.size(), 5u);
    for (const auto& value : result) {
        EXPECT_EQ(value, "hello");
    }
}

TEST_F(FixtureLiteral, EvaluateColumnHandlesInt16Literal) {
    Exec::LiteralExpression expr(Types::AnyPhysicalType{int16_t{640}}, kI16);

    const auto result = AsSpan<int16_t>(expr.EvaluateColumn(batch_, state_));

    ASSERT_EQ(result.size(), 5u);
    EXPECT_EQ(result[0], 640);
}

TEST_F(FixtureLiteral, EvaluateColumnHandlesBoolLiteral) {
    Exec::LiteralExpression expr(Types::AnyPhysicalType{uint8_t{1}}, kBool);

    const auto result = AsSpan<uint8_t>(expr.EvaluateColumn(batch_, state_));

    ASSERT_EQ(result.size(), 5u);
    EXPECT_EQ(result[0], 1);
}

TEST_F(FixtureLiteral, EvaluateScalarReturnsLiteralRegardlessOfRow) {
    Exec::LiteralExpression expr(Types::AnyPhysicalType{int64_t{1337}}, kI64);

    EXPECT_EQ(std::get<int64_t>(expr.EvaluateScalar(batch_, /*row=*/0)), 1337);
    EXPECT_EQ(std::get<int64_t>(expr.EvaluateScalar(batch_, /*row=*/4)), 1337);
}

TEST_F(FixtureLiteral, EvaluateScalarDoesNotTouchBatchData) {
    Exec::ExecBatch emptyBatch;
    Exec::LiteralExpression expr(Types::AnyPhysicalType{int32_t{55}}, kI32);

    EXPECT_EQ(std::get<int32_t>(expr.EvaluateScalar(emptyBatch, /*row=*/0)), 55)
        << "EvaluateScalar must work even on batches without a RowGroup";
}

TEST_F(FixtureLiteral, EvaluateScalarReturnsStringValue) {
    Exec::LiteralExpression expr(Types::AnyPhysicalType{std::string{"x"}}, kStr);

    EXPECT_EQ(std::get<std::string>(expr.EvaluateScalar(batch_, 0)), "x");
}

}  // namespace Columnar::Test
