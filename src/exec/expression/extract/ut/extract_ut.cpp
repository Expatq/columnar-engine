#include <exec/expression/extract/extract.h>

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
constexpr auto kI16 = Types::LogicalType::INT16;
constexpr auto kTs = Types::LogicalType::TIMESTAMP;

std::unique_ptr<Exec::IExpression> ColTs(std::string name) {
    return std::make_unique<Exec::ColumnRefExpression>(std::move(name), kTs);
}

std::unique_ptr<Exec::ExtractExpression> Ext(std::string colName, Exec::ExtractField field) {
    return std::make_unique<Exec::ExtractExpression>(ColTs(std::move(colName)), field);
}

Exec::ExecBatch MakeTsBatch(std::vector<int64_t> seconds) {
    return MakeBatch(MakeRowGroupOf(
        MakeSchema({{"ts", kTs}}),
        MakeColumn<int64_t>(std::move(seconds))));
}

std::span<const int16_t> AsInt16Span(const Exec::ColumnSpan& span) {
    return std::get<std::span<const int16_t>>(span);
}

class FixtureExtract : public ::testing::Test {
protected:
    Exec::EvalState state_;
};

}  // namespace

TEST_F(FixtureExtract, KindReportsExtract) {
    auto expr = Ext("ts", Exec::ExtractField::Minute);

    EXPECT_EQ(expr->Kind(), Exec::ExpressionKind::Extract);
}

TEST_F(FixtureExtract, ResultTypeIsInt16) {
    auto expr = Ext("ts", Exec::ExtractField::Hour);

    EXPECT_EQ(expr->ResultType(), kI16);
}

TEST_F(FixtureExtract, RequiredColumnsForwardsFromInput) {
    auto expr = Ext("event_time", Exec::ExtractField::Minute);

    const auto cols = expr->RequiredColumns();

    ASSERT_EQ(cols.size(), 1u);
    EXPECT_EQ(cols[0], "event_time");
}

TEST_F(FixtureExtract, ConstructorRejectsNullInput) {
    EXPECT_THROW(
        Exec::ExtractExpression(nullptr, Exec::ExtractField::Minute),
        std::invalid_argument);
}

TEST_F(FixtureExtract, ConstructorRejectsNonTimestampInput) {
    EXPECT_THROW(
        Exec::ExtractExpression(
            std::make_unique<Exec::ColumnRefExpression>("x", kI64),
            Exec::ExtractField::Minute),
        std::invalid_argument);
}

TEST_F(FixtureExtract, MinuteReturnsZeroAtMinuteBoundary) {
    auto batch = MakeTsBatch({0, 60, 120, 3600});
    auto expr = Ext("ts", Exec::ExtractField::Minute);

    const auto result = AsInt16Span(expr->EvaluateColumn(batch, state_));

    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
    EXPECT_EQ(result[2], 2);
    EXPECT_EQ(result[3], 0);
}

TEST_F(FixtureExtract, MinuteIsZeroToFiftyNineAcrossHourBoundary) {
    auto batch = MakeTsBatch({59, 60, 3540, 3599, 3600});
    auto expr = Ext("ts", Exec::ExtractField::Minute);

    const auto result = AsInt16Span(expr->EvaluateColumn(batch, state_));

    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
    EXPECT_EQ(result[2], 59);
    EXPECT_EQ(result[3], 59);
    EXPECT_EQ(result[4], 0);
}

TEST_F(FixtureExtract, HourWrapsAcrossDay) {
    auto batch = MakeTsBatch({0, 3600, 23 * 3600, 24 * 3600, 25 * 3600});
    auto expr = Ext("ts", Exec::ExtractField::Hour);

    const auto result = AsInt16Span(expr->EvaluateColumn(batch, state_));

    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 1);
    EXPECT_EQ(result[2], 23);
    EXPECT_EQ(result[3], 0);
    EXPECT_EQ(result[4], 1);
}

TEST_F(FixtureExtract, NegativeEpochMinuteWrapsAsFloorMod) {
    auto batch = MakeTsBatch({-1, -60, -61, -3600, -3661});
    auto expr = Ext("ts", Exec::ExtractField::Minute);

    const auto result = AsInt16Span(expr->EvaluateColumn(batch, state_));

    EXPECT_EQ(result[0], 59)
        << "ts=-1 is 1969-12-31 23:59:59 → minute 59 (not 0)";
    EXPECT_EQ(result[1], 59);
    EXPECT_EQ(result[2], 58);
    EXPECT_EQ(result[3], 0);
    EXPECT_EQ(result[4], 58)
        << "ts=-3661 is 1969-12-31 22:58:59 → minute 58";
}

TEST_F(FixtureExtract, NegativeEpochHourWrapsAsFloorMod) {
    auto batch = MakeTsBatch({-1, -3600, -3601, -86399, -86400});
    auto expr = Ext("ts", Exec::ExtractField::Hour);

    const auto result = AsInt16Span(expr->EvaluateColumn(batch, state_));

    EXPECT_EQ(result[0], 23)
        << "ts=-1 is 1969-12-31 23:59:59 → hour 23 (not 0)";
    EXPECT_EQ(result[1], 23);
    EXPECT_EQ(result[2], 22);
    EXPECT_EQ(result[3], 0);
    EXPECT_EQ(result[4], 0);
}

TEST_F(FixtureExtract, EvaluateScalarReturnsMinute) {
    auto batch = MakeTsBatch({3661});

    const auto value = std::get<int16_t>(
        Ext("ts", Exec::ExtractField::Minute)->EvaluateScalar(batch, /*row=*/0));

    EXPECT_EQ(value, 1);
}

TEST_F(FixtureExtract, EvaluateScalarOnNegativeEpochUsesFloorMod) {
    auto batch = MakeTsBatch({-1});

    EXPECT_EQ(std::get<int16_t>(Ext("ts", Exec::ExtractField::Minute)->EvaluateScalar(batch, 0)), 59);
    EXPECT_EQ(std::get<int16_t>(Ext("ts", Exec::ExtractField::Hour)->EvaluateScalar(batch, 0)), 23);
}

TEST_F(FixtureExtract, RespectsInputSelectionVector) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"ts", kTs}}),
        MakeColumn<int64_t>({0, 65, 130, 195, 260}));
    auto batch = MakeBatchWithSelection(rowGroup, {1, 3});

    const auto result = AsInt16Span(
        Ext("ts", Exec::ExtractField::Minute)->EvaluateColumn(batch, state_));

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 3);
}

TEST_F(FixtureExtract, RealisticTimestampMatchesClickHouseSemantics) {
    constexpr int64_t kTs2013Sample = 1373832000;
    auto batch = MakeTsBatch({kTs2013Sample});

    EXPECT_EQ(std::get<int16_t>(Ext("ts", Exec::ExtractField::Hour)->EvaluateScalar(batch, 0)), 20);
    EXPECT_EQ(std::get<int16_t>(Ext("ts", Exec::ExtractField::Minute)->EvaluateScalar(batch, 0)), 0);
}

}  // namespace Columnar::Test
