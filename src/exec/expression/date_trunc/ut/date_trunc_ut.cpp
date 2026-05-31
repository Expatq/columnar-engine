#include <exec/expression/date_trunc/date_trunc.h>

#include <exec/expression/column_ref/column_ref.h>

#include <exec/core/exec_batch.h>
#include <exec/interface/expression.h>

#include <tests/lib/data_builders.h>
#include <tests/lib/exec_batch_builders.h>

#include <util/calendar.h>

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
constexpr auto kTs = Types::LogicalType::TIMESTAMP;

std::unique_ptr<Exec::IExpression> ColTs(std::string name) {
    return std::make_unique<Exec::ColumnRefExpression>(std::move(name), kTs);
}

std::unique_ptr<Exec::DateTruncExpression> Trunc(std::string colName, Exec::DateTruncUnit unit) {
    return std::make_unique<Exec::DateTruncExpression>(ColTs(std::move(colName)), unit);
}

Exec::ExecBatch MakeTsBatch(std::vector<int64_t> seconds) {
    return MakeBatch(MakeRowGroupOf(
        MakeSchema({{"ts", kTs}}),
        MakeColumn<int64_t>(std::move(seconds))));
}

std::span<const int64_t> AsInt64Span(const Exec::ColumnSpan& span) {
    return std::get<std::span<const int64_t>>(span);
}

class FixtureDateTrunc : public ::testing::Test {
protected:
    Exec::EvalState state_;
};

}  // namespace

TEST_F(FixtureDateTrunc, KindReportsDateTrunc) {
    auto expr = Trunc("ts", Exec::DateTruncUnit::Minute);

    EXPECT_EQ(expr->Kind(), Exec::ExpressionKind::DateTrunc);
}

TEST_F(FixtureDateTrunc, ResultTypeIsTimestamp) {
    auto expr = Trunc("ts", Exec::DateTruncUnit::Hour);

    EXPECT_EQ(expr->ResultType(), kTs);
}

TEST_F(FixtureDateTrunc, RequiredColumnsForwardsFromInput) {
    auto expr = Trunc("event_time", Exec::DateTruncUnit::Day);

    const auto cols = expr->RequiredColumns();

    ASSERT_EQ(cols.size(), 1u);
    EXPECT_EQ(cols[0], "event_time");
}

TEST_F(FixtureDateTrunc, ConstructorRejectsNullInput) {
    EXPECT_THROW(
        Exec::DateTruncExpression(nullptr, Exec::DateTruncUnit::Minute),
        std::invalid_argument);
}

TEST_F(FixtureDateTrunc, ConstructorRejectsNonTimestampInput) {
    EXPECT_THROW(
        Exec::DateTruncExpression(
            std::make_unique<Exec::ColumnRefExpression>("x", kI64),
            Exec::DateTruncUnit::Minute),
        std::invalid_argument);
}

TEST_F(FixtureDateTrunc, MinuteRoundsDownInsideMinute) {
    auto batch = MakeTsBatch({0, 59, 60, 119, 120, 1234567});
    auto expr = Trunc("ts", Exec::DateTruncUnit::Minute);

    const auto result = AsInt64Span(expr->EvaluateColumn(batch, state_));

    ASSERT_EQ(result.size(), 6u);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 0);
    EXPECT_EQ(result[2], 60);
    EXPECT_EQ(result[3], 60);
    EXPECT_EQ(result[4], 120);
    EXPECT_EQ(result[5], (1234567 / 60) * 60);
}

TEST_F(FixtureDateTrunc, HourRoundsDownInsideHour) {
    auto batch = MakeTsBatch({0, 3599, 3600, 7199, 7200});
    auto expr = Trunc("ts", Exec::DateTruncUnit::Hour);

    const auto result = AsInt64Span(expr->EvaluateColumn(batch, state_));

    ASSERT_EQ(result.size(), 5u);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 0);
    EXPECT_EQ(result[2], 3600);
    EXPECT_EQ(result[3], 3600);
    EXPECT_EQ(result[4], 7200);
}

TEST_F(FixtureDateTrunc, DayRoundsDownInsideDay) {
    constexpr int64_t kDay = Calendar::kSecondsPerDay;
    auto batch = MakeTsBatch({0, kDay - 1, kDay, kDay + 1});
    auto expr = Trunc("ts", Exec::DateTruncUnit::Day);

    const auto result = AsInt64Span(expr->EvaluateColumn(batch, state_));

    ASSERT_EQ(result.size(), 4u);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 0);
    EXPECT_EQ(result[2], kDay);
    EXPECT_EQ(result[3], kDay);
}

TEST_F(FixtureDateTrunc, NegativeTimestampUsesFloorTowardNegativeInfinity) {
    auto batch = MakeTsBatch({-1, -60, -61, -3599, -3600});
    auto expr = Trunc("ts", Exec::DateTruncUnit::Minute);

    const auto result = AsInt64Span(expr->EvaluateColumn(batch, state_));

    ASSERT_EQ(result.size(), 5u);
    EXPECT_EQ(result[0], -60)
        << "ts=-1 belongs to minute starting at -60, not the one starting at 0";
    EXPECT_EQ(result[1], -60);
    EXPECT_EQ(result[2], -120);
    EXPECT_EQ(result[3], -3600);
    EXPECT_EQ(result[4], -3600);
}

TEST_F(FixtureDateTrunc, NegativeTimestampDayFloorIsCorrect) {
    constexpr int64_t kDay = Calendar::kSecondsPerDay;
    auto batch = MakeTsBatch({-1, -kDay, -kDay - 1});
    auto expr = Trunc("ts", Exec::DateTruncUnit::Day);

    const auto result = AsInt64Span(expr->EvaluateColumn(batch, state_));

    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], -kDay);
    EXPECT_EQ(result[1], -kDay);
    EXPECT_EQ(result[2], -2 * kDay);
}

TEST_F(FixtureDateTrunc, EvaluateScalarComputesSingleRow) {
    auto batch = MakeTsBatch({3661});
    auto expr = Trunc("ts", Exec::DateTruncUnit::Hour);

    const auto value = std::get<int64_t>(expr->EvaluateScalar(batch, /*row=*/0));

    EXPECT_EQ(value, 3600);
}

TEST_F(FixtureDateTrunc, EvaluateScalarOnNegativeTimestampFloorsCorrectly) {
    auto batch = MakeTsBatch({-1});
    auto expr = Trunc("ts", Exec::DateTruncUnit::Hour);

    const auto value = std::get<int64_t>(expr->EvaluateScalar(batch, /*row=*/0));

    EXPECT_EQ(value, -3600);
}

TEST_F(FixtureDateTrunc, RespectsInputSelectionVector) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"ts", kTs}}),
        MakeColumn<int64_t>({0, 65, 130, 195, 260}));
    auto batch = MakeBatchWithSelection(rowGroup, {1, 3});

    auto expr = Trunc("ts", Exec::DateTruncUnit::Minute);
    const auto result = AsInt64Span(expr->EvaluateColumn(batch, state_));

    ASSERT_EQ(result.size(), 2u)
        << "size of the output span must match selection size, not raw rowCount";
    EXPECT_EQ(result[0], 60);
    EXPECT_EQ(result[1], 180);
}

TEST_F(FixtureDateTrunc, AlignedTimestampIsLeftUnchanged) {
    auto batch = MakeTsBatch({60, 3600, Calendar::kSecondsPerDay});

    auto minuteResult = AsInt64Span(
        Trunc("ts", Exec::DateTruncUnit::Minute)->EvaluateColumn(batch, state_));
    EXPECT_EQ(minuteResult[0], 60);

    Exec::EvalState hourState;
    auto hourResult = AsInt64Span(
        Trunc("ts", Exec::DateTruncUnit::Hour)->EvaluateColumn(batch, hourState));
    EXPECT_EQ(hourResult[1], 3600);

    Exec::EvalState dayState;
    auto dayResult = AsInt64Span(
        Trunc("ts", Exec::DateTruncUnit::Day)->EvaluateColumn(batch, dayState));
    EXPECT_EQ(dayResult[2], Calendar::kSecondsPerDay);
}

}  // namespace Columnar::Test
