#include <exec/aggregate/lib/aggregate_result.h>
#include <exec/aggregate/lib/consumers.h>
#include <exec/aggregate/lib/spec.h>
#include <exec/aggregate/lib/state.h>

#include <exec/result_format/row_group_builder.h>

#include <core/row_group.h>
#include <core/schema.h>
#include <core/types.h>

#include <util/int128.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace Columnar::Test {

namespace {

constexpr auto kI16 = Types::LogicalType::INT16;
constexpr auto kI32 = Types::LogicalType::INT32;
constexpr auto kI64 = Types::LogicalType::INT64;
constexpr auto kStr = Types::LogicalType::STRING;

RowGroup BuildAndFinish(Exec::AggregateState state, Types::LogicalType outputType) {
    Schema schema;
    schema.AddColumn("v", outputType);
    Exec::RowGroupBuilder builder(std::move(schema));
    Exec::AppendAggregateResult(builder, /*colIdx=*/0, state);
    return builder.Finish();
}

template <typename T>
T SingleValue(const RowGroup& rg) {
    return rg.GetColumn(0).GetTypedData<T>().front();
}

}  // namespace

TEST(AvgState, EmptyStateReturnsZero) {
    Exec::AvgState s;

    EXPECT_EQ(s.Result(), 0);
}

TEST(AvgState, SimplePositiveIntegerDivisionTruncates) {
    Exec::AvgState s{.sum = Int128(10), .count = 3};

    EXPECT_EQ(s.Result(), 3) << "10/3 = 3 (integer truncation, not 3.33 rounded)";
}

TEST(AvgState, NegativeSumTruncatesTowardZero) {
    Exec::AvgState s{.sum = Int128(-10), .count = 3};

    EXPECT_EQ(s.Result(), -3) << "-10/3 = -3 (C++ truncation toward zero)";
}

TEST(AvgState, ExactDivisionForRoundResult) {
    Exec::AvgState s{.sum = Int128(100), .count = 5};

    EXPECT_EQ(s.Result(), 20);
}

TEST(AvgState, LargeInt64SumPreservesFullPrecision) {
    constexpr int64_t kPerRow = 1948165676197850120LL;
    constexpr uint64_t kRows = 1000;
    Exec::AvgState s{.sum = Int128(kPerRow) * Int128(kRows), .count = kRows};

    EXPECT_EQ(s.Result(), kPerRow)
        << "Q3 regression: large UserID values must avg exactly, not lose low bits via double";
}

TEST(AvgState, SumThatExceedsInt64StillDividesCorrectly) {
    const Int128 perRow = Int128(std::numeric_limits<int64_t>::max());
    Exec::AvgState s{.sum = perRow * Int128(10), .count = 10};

    EXPECT_EQ(s.Result(), std::numeric_limits<int64_t>::max())
        << "sum overflows int64 but stays in Int128; integer divide back to int64";
}

TEST(CountState, IncrementAccumulates) {
    Exec::CountState s;
    ++s.count;
    s.count += 5;

    EXPECT_EQ(s.count, 6u);
}

TEST(SumState, Int128AccumulationWithoutOverflowOnLargeValues) {
    Exec::SumState<Int128> s;
    const Int128 big = Int128(std::numeric_limits<int64_t>::max());
    for (int i = 0; i < 100; ++i) {
        s.sum += big;
    }

    EXPECT_EQ(Int128ToString(s.sum), Int128ToString(big * Int128(100)));
}

TEST(MinMaxState, TracksSmallestAcrossInsertions) {
    Exec::MinMaxState<int64_t> s;
    for (int64_t v : {5, 3, 8, 1, 4}) {
        if (!s.value.has_value() || v < *s.value) {
            s.value = v;
        }
    }

    EXPECT_EQ(*s.value, 1);
}

TEST(MinMaxState, OptionalRemainsUnsetForEmptyInput) {
    Exec::MinMaxState<int64_t> s;

    EXPECT_FALSE(s.value.has_value());
}

TEST(CountDistinctState, DeduplicatesInsertedValues) {
    Exec::CountDistinctState<int64_t> s;
    for (int64_t v : {1, 2, 2, 3, 1, 4, 4}) {
        s.seen.insert(v);
    }

    EXPECT_EQ(s.seen.size(), 4u);
}

TEST(ConsumeTyped, SumAccumulatesInt64IntoInt128) {
    Exec::AggregateState state = Exec::SumState<Int128>{};
    const std::vector<int64_t> values{10, 20, 30, 40};

    Exec::ConsumeTyped<int64_t, Int128>(
        std::span<const int64_t>{values}, Exec::AggregateKind::Sum, state);

    EXPECT_EQ(static_cast<int64_t>(std::get<Exec::SumState<Int128>>(state).sum), 100);
}

TEST(ConsumeTyped, AvgAccumulatesSumAndCount) {
    Exec::AggregateState state = Exec::AvgState{};
    const std::vector<int64_t> values{2, 4, 6};

    Exec::ConsumeTyped<int64_t, Int128>(
        std::span<const int64_t>{values}, Exec::AggregateKind::Avg, state);

    const auto& avg = std::get<Exec::AvgState>(state);
    EXPECT_EQ(static_cast<int64_t>(avg.sum), 12);
    EXPECT_EQ(avg.count, 3u);
    EXPECT_EQ(avg.Result(), 4);
}

TEST(ConsumeTyped, MinTracksSmallest) {
    Exec::AggregateState state = Exec::MinMaxState<int32_t>{};
    const std::vector<int32_t> values{5, 1, 9, 3};

    Exec::ConsumeTyped<int32_t, Int128>(
        std::span<const int32_t>{values}, Exec::AggregateKind::Min, state);

    EXPECT_EQ(*std::get<Exec::MinMaxState<int32_t>>(state).value, 1);
}

TEST(ConsumeTyped, MaxTracksLargest) {
    Exec::AggregateState state = Exec::MinMaxState<int32_t>{};
    const std::vector<int32_t> values{5, 1, 9, 3};

    Exec::ConsumeTyped<int32_t, Int128>(
        std::span<const int32_t>{values}, Exec::AggregateKind::Max, state);

    EXPECT_EQ(*std::get<Exec::MinMaxState<int32_t>>(state).value, 9);
}

TEST(ConsumeTyped, EmptySpanLeavesStateUnchanged) {
    Exec::AggregateState state = Exec::AvgState{.sum = Int128(7), .count = 1};
    const std::vector<int64_t> empty;

    Exec::ConsumeTyped<int64_t, Int128>(
        std::span<const int64_t>{empty}, Exec::AggregateKind::Avg, state);

    const auto& avg = std::get<Exec::AvgState>(state);
    EXPECT_EQ(static_cast<int64_t>(avg.sum), 7);
    EXPECT_EQ(avg.count, 1u);
}

TEST(ConsumeCountDistinct, DeduplicatesStringValues) {
    Exec::CountDistinctState<std::string> state;
    const std::vector<std::string> values{"foo", "bar", "foo", "baz", "bar"};

    Exec::ConsumeCountDistinct(Exec::ColumnSpan{std::span<const std::string>{values}}, state);

    EXPECT_EQ(state.seen.size(), 3u);
}

TEST(AppendAggregateResult, CountStateBecomesInt64Row) {
    auto rg = BuildAndFinish(Exec::CountState{.count = 42}, kI64);

    EXPECT_EQ(SingleValue<int64_t>(rg), 42);
}

TEST(AppendAggregateResult, SumStateInt128TruncatesToInt64) {
    auto rg = BuildAndFinish(Exec::SumState<Int128>{.sum = Int128(12345)}, kI64);

    EXPECT_EQ(SingleValue<int64_t>(rg), 12345);
}

TEST(AppendAggregateResult, AvgStateProducesIntegerDivisionResult) {
    auto rg = BuildAndFinish(Exec::AvgState{.sum = Int128(100), .count = 7}, kI64);

    EXPECT_EQ(SingleValue<int64_t>(rg), 14) << "100/7 = 14 (truncated)";
}

TEST(AppendAggregateResult, AvgEmptyProducesZero) {
    auto rg = BuildAndFinish(Exec::AvgState{}, kI64);

    EXPECT_EQ(SingleValue<int64_t>(rg), 0);
}

TEST(AppendAggregateResult, CountDistinctReturnsSetSize) {
    Exec::CountDistinctState<int64_t> dist;
    dist.seen.insert(1);
    dist.seen.insert(2);
    dist.seen.insert(3);
    auto rg = BuildAndFinish(dist, kI64);

    EXPECT_EQ(SingleValue<int64_t>(rg), 3);
}

TEST(AppendAggregateResult, MinMaxUnsetReturnsDefaultZeroForInt) {
    auto rg = BuildAndFinish(Exec::MinMaxState<int64_t>{}, kI64);

    EXPECT_EQ(SingleValue<int64_t>(rg), 0)
        << "engine lacks NULL — empty group falls back to default-constructed value";
}

TEST(AppendAggregateResult, MinMaxStringSetReturnsString) {
    Exec::MinMaxState<std::string> mm{.value = std::string{"hello"}};
    auto rg = BuildAndFinish(mm, kStr);

    EXPECT_EQ(SingleValue<std::string>(rg), "hello");
}

TEST(AppendAggregateResult, MinMaxInt16PropagatesType) {
    Exec::MinMaxState<int16_t> mm{.value = int16_t{-42}};
    auto rg = BuildAndFinish(mm, kI16);

    EXPECT_EQ(SingleValue<int16_t>(rg), -42);
}

TEST(AppendAggregateResult, MinMaxInt32PropagatesType) {
    Exec::MinMaxState<int32_t> mm{.value = int32_t{1234567}};
    auto rg = BuildAndFinish(mm, kI32);

    EXPECT_EQ(SingleValue<int32_t>(rg), 1234567);
}

}  // namespace Columnar::Test
