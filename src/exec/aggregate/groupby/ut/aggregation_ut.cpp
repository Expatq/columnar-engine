#include <exec/aggregate/groupby/aggregation.h>

#include <exec/aggregate/lib/spec.h>

#include <exec/expression/column_ref/column_ref.h>

#include <core/row_group.h>

#include <tests/lib/data_builders.h>
#include <tests/lib/exec_batch_builders.h>
#include <tests/lib/fake_operator.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace Columnar::Test {

namespace {

constexpr auto kI32 = Types::LogicalType::INT32;
constexpr auto kI64 = Types::LogicalType::INT64;
constexpr auto kStr = Types::LogicalType::STRING;

std::unique_ptr<Exec::IExpression> Col(std::string name, Types::LogicalType type) {
    return std::make_unique<Exec::ColumnRefExpression>(std::move(name), type);
}

Exec::GroupByKey GbKey(std::string name, Types::LogicalType type) {
    return Exec::GroupByKey{Col(name, type), name};
}

template <typename... Args>
std::vector<Exec::GroupByKey> KeysOf(Args&&... ks) {
    std::vector<Exec::GroupByKey> v;
    v.reserve(sizeof...(ks));
    (v.emplace_back(std::forward<Args>(ks)), ...);
    return v;
}

template <typename... Args>
std::vector<Exec::AggregateSpec> AggsOf(Args&&... as) {
    std::vector<Exec::AggregateSpec> v;
    v.reserve(sizeof...(as));
    (v.emplace_back(std::forward<Args>(as)), ...);
    return v;
}

template <typename... Args>
std::vector<Exec::ExecBatch> Batches(Args&&... bs) {
    std::vector<Exec::ExecBatch> v;
    v.reserve(sizeof...(bs));
    (v.emplace_back(std::forward<Args>(bs)), ...);
    return v;
}

std::shared_ptr<RowGroup> RunGroupBy(std::vector<Exec::ExecBatch> batches,
                                     std::vector<Exec::GroupByKey> keys,
                                     std::vector<Exec::AggregateSpec> aggregates) {
    Exec::GroupByAggregation op(
        std::make_unique<FakeOperator>(std::move(batches)),
        std::move(keys), std::move(aggregates));
    op.Open();
    Exec::ExecBatch out;
    op.Next(out);
    op.Close();
    return out.rowGroup;
}

template <typename T>
std::vector<T> Values(const RowGroup& rg, const std::string& col) {
    const auto& vec = rg.FindColumn(col)->GetTypedData<T>();
    return {vec.begin(), vec.end()};
}

template <typename K, typename V>
std::vector<std::pair<K, V>> SortedKV(const RowGroup& rg,
                                      const std::string& keyCol,
                                      const std::string& valCol) {
    const auto keys = Values<K>(rg, keyCol);
    const auto vals = Values<V>(rg, valCol);
    std::vector<std::pair<K, V>> out;
    out.reserve(keys.size());
    for (size_t i = 0; i < keys.size(); ++i) {
        out.emplace_back(keys[i], vals[i]);
    }
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace

TEST(GroupByAggregation, ConstructorRejectsNullChild) {
    EXPECT_THROW(
        Exec::GroupByAggregation(nullptr, KeysOf(GbKey("x", kI64)), AggsOf(Exec::CountStar("c"))),
        std::invalid_argument);
}

TEST(GroupByAggregation, ConstructorRejectsEmptyKeys) {
    EXPECT_THROW(
        Exec::GroupByAggregation(
            std::make_unique<FakeOperator>(Batches()), {}, AggsOf(Exec::CountStar("c"))),
        std::invalid_argument);
}

TEST(GroupByAggregation, ConstructorRejectsEmptyAggregates) {
    EXPECT_THROW(
        Exec::GroupByAggregation(
            std::make_unique<FakeOperator>(Batches()),
            KeysOf(GbKey("x", kI64)),
            {}),
        std::invalid_argument);
}

TEST(GroupByAggregation, SingleInt64KeyCountStarReturnsCountsPerGroup) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 1, 1, 2, 3})));

    auto rg = RunGroupBy(Batches(std::move(batch)),
                         KeysOf(GbKey("x", kI64)),
                         AggsOf(Exec::CountStar("c")));

    EXPECT_EQ((SortedKV<int64_t, int64_t>(*rg, "x", "c")),
              (std::vector<std::pair<int64_t, int64_t>>{{1, 3}, {2, 2}, {3, 1}}));
}

TEST(GroupByAggregation, SumAggregateAccumulatesPerGroup) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"k", kI64}, {"v", kI64}}),
        MakeColumn<int64_t>({1, 2, 1, 2, 1}),
        MakeColumn<int64_t>({10, 100, 20, 200, 30})));

    auto rg = RunGroupBy(Batches(std::move(batch)),
                         KeysOf(GbKey("k", kI64)),
                         AggsOf(Exec::Sum(Col("v", kI64), "s", kI64)));

    EXPECT_EQ((SortedKV<int64_t, int64_t>(*rg, "k", "s")),
              (std::vector<std::pair<int64_t, int64_t>>{{1, 60}, {2, 300}}));
}

TEST(GroupByAggregation, AvgAggregatePreservesInt64PrecisionAcrossLargeValues) {
    constexpr int64_t kBigVal = 1948165676197850120LL;
    std::vector<int64_t> userIds(100, kBigVal);
    std::vector<int64_t> groupKeys(100, 1);

    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"g", kI64}, {"u", kI64}}),
        MakeColumn<int64_t>(std::move(groupKeys)),
        MakeColumn<int64_t>(std::move(userIds))));

    auto rg = RunGroupBy(Batches(std::move(batch)),
                         KeysOf(GbKey("g", kI64)),
                         AggsOf(Exec::Avg(Col("u", kI64), "avg_u", kI64)));

    EXPECT_EQ((SortedKV<int64_t, int64_t>(*rg, "g", "avg_u")),
              (std::vector<std::pair<int64_t, int64_t>>{{1, kBigVal}}))
        << "Q3 regression: Avg over large int64 values must not lose precision through GroupBy";
}

TEST(GroupByAggregation, MultiInt64KeysUseInt128ModeAndDistinguishGroups) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"a", kI64}, {"b", kI64}}),
        MakeColumn<int64_t>({1, 1, 2, 2, 1}),
        MakeColumn<int64_t>({10, 20, 10, 20, 10})));

    auto rg = RunGroupBy(Batches(std::move(batch)),
                         KeysOf(GbKey("a", kI64), GbKey("b", kI64)),
                         AggsOf(Exec::CountStar("c")));

    ASSERT_EQ(rg->GetRowCount(), 4u);
    const auto a = Values<int64_t>(*rg, "a");
    const auto b = Values<int64_t>(*rg, "b");
    const auto c = Values<int64_t>(*rg, "c");
    std::vector<std::tuple<int64_t, int64_t, int64_t>> rows;
    for (size_t i = 0; i < a.size(); ++i) {
        rows.emplace_back(a[i], b[i], c[i]);
    }
    std::sort(rows.begin(), rows.end());

    EXPECT_EQ(rows,
              (std::vector<std::tuple<int64_t, int64_t, int64_t>>{
                  {1, 10, 2}, {1, 20, 1}, {2, 10, 1}, {2, 20, 1}}));
}

TEST(GroupByAggregation, SingleStringKeyTriggersInlineModeAndCountsCorrectly) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"name", kStr}}),
        MakeColumn<std::string>({"alice", "bob", "alice", "carol", "alice"})));

    auto rg = RunGroupBy(Batches(std::move(batch)),
                         KeysOf(GbKey("name", kStr)),
                         AggsOf(Exec::CountStar("c")));

    EXPECT_EQ((SortedKV<std::string, int64_t>(*rg, "name", "c")),
              (std::vector<std::pair<std::string, int64_t>>{
                  {"alice", 3}, {"bob", 1}, {"carol", 1}}));
}

TEST(GroupByAggregation, LongStringKeyExceedingPrefixUsesArenaCorrectly) {
    const std::string a(50, 'a');
    const std::string b(50, 'b');
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"name", kStr}}),
        MakeColumn<std::string>({a, b, a, a, b})));

    auto rg = RunGroupBy(Batches(std::move(batch)),
                         KeysOf(GbKey("name", kStr)),
                         AggsOf(Exec::CountStar("c")));

    EXPECT_EQ((SortedKV<std::string, int64_t>(*rg, "name", "c")),
              (std::vector<std::pair<std::string, int64_t>>{{a, 3}, {b, 2}}));
}

TEST(GroupByAggregation, MixedStringAndIntKeysGroupedAsComposite) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"name", kStr}, {"k", kI64}}),
        MakeColumn<std::string>({"a", "a", "b", "a", "b"}),
        MakeColumn<int64_t>({1, 1, 1, 2, 2})));

    auto rg = RunGroupBy(Batches(std::move(batch)),
                         KeysOf(GbKey("name", kStr), GbKey("k", kI64)),
                         AggsOf(Exec::CountStar("c")));

    ASSERT_EQ(rg->GetRowCount(), 4u);
    const auto names = Values<std::string>(*rg, "name");
    const auto ks = Values<int64_t>(*rg, "k");
    const auto cs = Values<int64_t>(*rg, "c");
    std::vector<std::tuple<std::string, int64_t, int64_t>> rows;
    for (size_t i = 0; i < names.size(); ++i) {
        rows.emplace_back(names[i], ks[i], cs[i]);
    }
    std::sort(rows.begin(), rows.end());

    EXPECT_EQ(rows,
              (std::vector<std::tuple<std::string, int64_t, int64_t>>{
                  {"a", 1, 2}, {"a", 2, 1}, {"b", 1, 1}, {"b", 2, 1}}));
}

TEST(GroupByAggregation, NegativeIntegerKeysRoundTripCorrectly) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI32}}),
        MakeColumn<int32_t>({-100, -100, 0, 5, -100, 5})));

    auto rg = RunGroupBy(Batches(std::move(batch)),
                         KeysOf(GbKey("x", kI32)),
                         AggsOf(Exec::CountStar("c")));

    EXPECT_EQ((SortedKV<int32_t, int64_t>(*rg, "x", "c")),
              (std::vector<std::pair<int32_t, int64_t>>{{-100, 3}, {0, 1}, {5, 2}}));
}

TEST(GroupByAggregation, MultiAggregateAllReturnTheirOwnValuePerGroup) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"k", kI64}, {"v", kI64}}),
        MakeColumn<int64_t>({1, 1, 1, 2, 2}),
        MakeColumn<int64_t>({3, 9, 6, 100, 200})));

    auto rg = RunGroupBy(Batches(std::move(batch)),
                         KeysOf(GbKey("k", kI64)),
                         AggsOf(
                             Exec::CountStar("c"),
                             Exec::Sum(Col("v", kI64), "s", kI64),
                             Exec::Min(Col("v", kI64), "mn"),
                             Exec::Max(Col("v", kI64), "mx"),
                             Exec::Avg(Col("v", kI64), "av", kI64)));

    auto ks = Values<int64_t>(*rg, "k");
    auto cs = Values<int64_t>(*rg, "c");
    auto ss = Values<int64_t>(*rg, "s");
    auto mns = Values<int64_t>(*rg, "mn");
    auto mxs = Values<int64_t>(*rg, "mx");
    auto avs = Values<int64_t>(*rg, "av");

    std::vector<size_t> order(ks.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return ks[a] < ks[b]; });

    ASSERT_EQ(ks.size(), 2u);
    const size_t i1 = order[0];
    const size_t i2 = order[1];
    EXPECT_EQ(ks[i1], 1);
    EXPECT_EQ(cs[i1], 3);
    EXPECT_EQ(ss[i1], 18);
    EXPECT_EQ(mns[i1], 3);
    EXPECT_EQ(mxs[i1], 9);
    EXPECT_EQ(avs[i1], 6);

    EXPECT_EQ(ks[i2], 2);
    EXPECT_EQ(cs[i2], 2);
    EXPECT_EQ(ss[i2], 300);
    EXPECT_EQ(mns[i2], 100);
    EXPECT_EQ(mxs[i2], 200);
    EXPECT_EQ(avs[i2], 150);
}

TEST(GroupByAggregation, CountDistinctOnInt64) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"k", kI64}, {"v", kI64}}),
        MakeColumn<int64_t>({1, 1, 1, 2, 2, 2}),
        MakeColumn<int64_t>({10, 20, 10, 5, 5, 7})));

    auto rg = RunGroupBy(Batches(std::move(batch)),
                         KeysOf(GbKey("k", kI64)),
                         AggsOf(Exec::CountDistinct(Col("v", kI64), "u")));

    EXPECT_EQ((SortedKV<int64_t, int64_t>(*rg, "k", "u")),
              (std::vector<std::pair<int64_t, int64_t>>{{1, 2}, {2, 2}}));
}

TEST(GroupByAggregation, CountDistinctOnString) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"k", kI64}, {"name", kStr}}),
        MakeColumn<int64_t>({1, 1, 1, 2, 2}),
        MakeColumn<std::string>({"a", "b", "a", "x", "x"})));

    auto rg = RunGroupBy(Batches(std::move(batch)),
                         KeysOf(GbKey("k", kI64)),
                         AggsOf(Exec::CountDistinct(Col("name", kStr), "u")));

    EXPECT_EQ((SortedKV<int64_t, int64_t>(*rg, "k", "u")),
              (std::vector<std::pair<int64_t, int64_t>>{{1, 2}, {2, 1}}));
}

TEST(GroupByAggregation, MinMaxOnStringColumn) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"k", kI64}, {"s", kStr}}),
        MakeColumn<int64_t>({1, 1, 1, 2}),
        MakeColumn<std::string>({"banana", "apple", "cherry", "zzz"})));

    auto rg = RunGroupBy(Batches(std::move(batch)),
                         KeysOf(GbKey("k", kI64)),
                         AggsOf(
                             Exec::Min(Col("s", kStr), "mn"),
                             Exec::Max(Col("s", kStr), "mx")));

    auto ks = Values<int64_t>(*rg, "k");
    auto mns = Values<std::string>(*rg, "mn");
    auto mxs = Values<std::string>(*rg, "mx");

    std::vector<std::tuple<int64_t, std::string, std::string>> rows;
    for (size_t i = 0; i < ks.size(); ++i) {
        rows.emplace_back(ks[i], mns[i], mxs[i]);
    }
    std::sort(rows.begin(), rows.end());

    EXPECT_EQ(rows,
              (std::vector<std::tuple<int64_t, std::string, std::string>>{
                  {1, "apple", "cherry"}, {2, "zzz", "zzz"}}));
}

TEST(GroupByAggregation, AccumulatesAcrossMultipleBatches) {
    auto b1 = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"k", kI64}, {"v", kI64}}),
        MakeColumn<int64_t>({1, 2}),
        MakeColumn<int64_t>({10, 20})));
    auto b2 = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"k", kI64}, {"v", kI64}}),
        MakeColumn<int64_t>({1, 1}),
        MakeColumn<int64_t>({30, 40})));

    auto rg = RunGroupBy(Batches(std::move(b1), std::move(b2)),
                         KeysOf(GbKey("k", kI64)),
                         AggsOf(Exec::Sum(Col("v", kI64), "s", kI64)));

    EXPECT_EQ((SortedKV<int64_t, int64_t>(*rg, "k", "s")),
              (std::vector<std::pair<int64_t, int64_t>>{{1, 80}, {2, 20}}));
}

TEST(GroupByAggregation, RespectsInputSelectionVector) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"k", kI64}, {"v", kI64}}),
        MakeColumn<int64_t>({1, 2, 1, 2, 1}),
        MakeColumn<int64_t>({10, 20, 30, 40, 50}));
    auto batch = MakeBatchWithSelection(rowGroup, {1, 3});

    auto rg = RunGroupBy(Batches(std::move(batch)),
                         KeysOf(GbKey("k", kI64)),
                         AggsOf(Exec::Sum(Col("v", kI64), "s", kI64)));

    EXPECT_EQ((SortedKV<int64_t, int64_t>(*rg, "k", "s")),
              (std::vector<std::pair<int64_t, int64_t>>{{2, 60}}))
        << "selection {1,3} keeps only the two rows with k=2; rows with k=1 are invisible";
}

TEST(GroupByAggregation, OutputSchemaPlacesKeysBeforeAggregates) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"k", kI64}, {"v", kI64}}),
        MakeColumn<int64_t>({1}),
        MakeColumn<int64_t>({10})));

    auto rg = RunGroupBy(Batches(std::move(batch)),
                         KeysOf(GbKey("k", kI64)),
                         AggsOf(
                             Exec::CountStar("c"),
                             Exec::Sum(Col("v", kI64), "s", kI64)));

    ASSERT_EQ(rg->GetColumnCount(), 3u);
    EXPECT_EQ(rg->GetSchema().GetColumn(0).name, "k");
    EXPECT_EQ(rg->GetSchema().GetColumn(1).name, "c");
    EXPECT_EQ(rg->GetSchema().GetColumn(2).name, "s");
}

TEST(GroupByAggregation, ReopenResetsAccumulatedState) {
    auto b1 = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"k", kI64}}),
        MakeColumn<int64_t>({1, 1})));

    Exec::GroupByAggregation op(
        std::make_unique<FakeOperator>(Batches(std::move(b1))),
        KeysOf(GbKey("k", kI64)),
        AggsOf(Exec::CountStar("c")));

    op.Open();
    Exec::ExecBatch first;
    op.Next(first);
    op.Close();

    op.Open();
    Exec::ExecBatch second;
    op.Next(second);
    op.Close();

    EXPECT_EQ((SortedKV<int64_t, int64_t>(*second.rowGroup, "k", "c")),
              (std::vector<std::pair<int64_t, int64_t>>{{1, 2}}))
        << "reopen must restart counts from zero, not accumulate from previous run";
}

}  // namespace Columnar::Test
