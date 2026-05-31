#include <exec/expression/string_len/string_len.h>

#include <exec/expression/column_ref/column_ref.h>

#include <exec/core/exec_batch.h>
#include <exec/interface/expression.h>

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
constexpr auto kStr = Types::LogicalType::STRING;

std::unique_ptr<Exec::IExpression> ColStr(std::string name) {
    return std::make_unique<Exec::ColumnRefExpression>(std::move(name), kStr);
}

std::unique_ptr<Exec::StringLenExpression> StrLen(std::string colName) {
    return std::make_unique<Exec::StringLenExpression>(ColStr(std::move(colName)));
}

Exec::ExecBatch MakeStringBatch(std::vector<std::string> values) {
    return MakeBatch(MakeRowGroupOf(
        MakeSchema({{"s", kStr}}),
        MakeColumn<std::string>(std::move(values))));
}

std::span<const int64_t> AsInt64Span(const Exec::ColumnSpan& span) {
    return std::get<std::span<const int64_t>>(span);
}

class FixtureStringLen : public ::testing::Test {
protected:
    Exec::EvalState state_;
};

}  // namespace

TEST_F(FixtureStringLen, KindReportsStringLen) {
    auto expr = StrLen("s");

    EXPECT_EQ(expr->Kind(), Exec::ExpressionKind::StringLen);
}

TEST_F(FixtureStringLen, ResultTypeIsInt64) {
    auto expr = StrLen("s");

    EXPECT_EQ(expr->ResultType(), kI64);
}

TEST_F(FixtureStringLen, RequiredColumnsForwardsFromInput) {
    auto expr = StrLen("url");

    const auto cols = expr->RequiredColumns();

    ASSERT_EQ(cols.size(), 1u);
    EXPECT_EQ(cols[0], "url");
}

TEST_F(FixtureStringLen, ConstructorRejectsNullInput) {
    EXPECT_THROW(Exec::StringLenExpression(nullptr), std::invalid_argument);
}

TEST_F(FixtureStringLen, ConstructorRejectsNonStringInput) {
    EXPECT_THROW(
        Exec::StringLenExpression(
            std::make_unique<Exec::ColumnRefExpression>("x", kI64)),
        std::invalid_argument);
}

TEST_F(FixtureStringLen, ReturnsByteLengthsForRowsOfVaryingSize) {
    auto batch = MakeStringBatch({"", "a", "abc", "hello world"});

    const auto result = AsInt64Span(StrLen("s")->EvaluateColumn(batch, state_));

    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
    EXPECT_EQ(result[2], 3);
    EXPECT_EQ(result[3], 11);
}

TEST_F(FixtureStringLen, CountsBytesNotUnicodeCodepoints) {
    auto batch = MakeStringBatch({"тест", "😀", "abç"});

    const auto result = AsInt64Span(StrLen("s")->EvaluateColumn(batch, state_));

    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], 8) << "4 cyrillic chars × 2 bytes each";
    EXPECT_EQ(result[1], 4) << "emoji U+1F600 takes 4 bytes in UTF-8";
    EXPECT_EQ(result[2], 4) << "'a' (1) + 'b' (1) + 'ç' (2) = 4 bytes";
}

TEST_F(FixtureStringLen, EmbeddedNullBytesAreCounted) {
    std::string withNul{"a\0b", 3};

    auto batch = MakeStringBatch({withNul});

    const auto result = AsInt64Span(StrLen("s")->EvaluateColumn(batch, state_));

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], 3) << "embedded NUL must not terminate counting";
}

TEST_F(FixtureStringLen, EvaluateScalarReturnsByteLength) {
    auto batch = MakeStringBatch({"foo", "bars"});

    EXPECT_EQ(std::get<int64_t>(StrLen("s")->EvaluateScalar(batch, /*row=*/0)), 3);
    EXPECT_EQ(std::get<int64_t>(StrLen("s")->EvaluateScalar(batch, /*row=*/1)), 4);
}

TEST_F(FixtureStringLen, RespectsInputSelectionVectorSizesOutputAccordingly) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"s", kStr}}),
        MakeColumn<std::string>({"alpha", "be", "gamma", "d", "epsilon"}));
    auto batch = MakeBatchWithSelection(rowGroup, {1, 3});

    const auto result = AsInt64Span(StrLen("s")->EvaluateColumn(batch, state_));

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], 2);
    EXPECT_EQ(result[1], 1);
}

TEST_F(FixtureStringLen, EmptyBatchReturnsEmptySpan) {
    auto batch = MakeStringBatch({});

    EXPECT_TRUE(AsInt64Span(StrLen("s")->EvaluateColumn(batch, state_)).empty());
}

TEST_F(FixtureStringLen, LargeStringLengthFitsInInt64) {
    std::string big(100'000, 'x');
    auto batch = MakeStringBatch({big});

    const auto result = AsInt64Span(StrLen("s")->EvaluateColumn(batch, state_));

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], 100'000);
}

}  // namespace Columnar::Test
