#include <exec/sort/top_k.h>

#include <exec/expression/column_ref/column_ref.h>

#include <core/row_group.h>

#include <tests/lib/data_builders.h>
#include <tests/lib/exec_batch_builders.h>
#include <tests/lib/fake_operator.h>

#include <gtest/gtest.h>

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

Exec::SortKey Asc(std::string col, Types::LogicalType type) {
    return Exec::SortKey{
        std::make_unique<Exec::ColumnRefExpression>(std::move(col), type),
        false};
}

Exec::SortKey Desc(std::string col, Types::LogicalType type) {
    return Exec::SortKey{
        std::make_unique<Exec::ColumnRefExpression>(std::move(col), type),
        true};
}

template <typename... Args>
std::vector<Exec::SortKey> Keys(Args&&... ks) {
    std::vector<Exec::SortKey> v;
    v.reserve(sizeof...(ks));
    (v.emplace_back(std::forward<Args>(ks)), ...);
    return v;
}

template <typename... Args>
std::vector<Exec::ExecBatch> Batches(Args&&... bs) {
    std::vector<Exec::ExecBatch> v;
    v.reserve(sizeof...(bs));
    (v.emplace_back(std::forward<Args>(bs)), ...);
    return v;
}

std::shared_ptr<RowGroup> RunTopK(std::vector<Exec::ExecBatch> batches,
                                  std::vector<Exec::SortKey> keys,
                                  size_t limit,
                                  size_t offset = 0) {
    Exec::TopK op(std::make_unique<FakeOperator>(std::move(batches)),
                  std::move(keys), limit, offset);
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

}  // namespace

TEST(TopK, ConstructorRejectsNullChild) {
    EXPECT_THROW(
        Exec::TopK(nullptr, Keys(Asc("x", kI64)), 10),
        std::invalid_argument);
}

TEST(TopK, ConstructorRejectsEmptyKeys) {
    EXPECT_THROW(
        Exec::TopK(std::make_unique<FakeOperator>(Batches()),
                   {}, /*limit=*/10),
        std::invalid_argument);
}

TEST(TopK, ConstructorRejectsZeroLimit) {
    EXPECT_THROW(
        Exec::TopK(std::make_unique<FakeOperator>(Batches()),
                   Keys(Asc("x", kI64)), /*limit=*/0),
        std::invalid_argument);
}

TEST(TopK, EmptyInputProducesEmptyOutput) {
    Exec::TopK op(std::make_unique<FakeOperator>(Batches()),
                  Keys(Asc("x", kI64)), 10);
    op.Open();
    Exec::ExecBatch out;
    const bool produced = op.Next(out);
    op.Close();

    EXPECT_FALSE(produced);
}

TEST(TopK, OrderByAscReturnsSortedAscending) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({30, 10, 50, 20, 40})));

    auto rg = RunTopK(Batches(std::move(batch)), Keys(Asc("x", kI64)), 10);

    EXPECT_EQ(Values<int64_t>(*rg, "x"),
              (std::vector<int64_t>{10, 20, 30, 40, 50}));
}

TEST(TopK, OrderByDescReturnsSortedDescending) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({30, 10, 50, 20, 40})));

    auto rg = RunTopK(Batches(std::move(batch)), Keys(Desc("x", kI64)), 10);

    EXPECT_EQ(Values<int64_t>(*rg, "x"),
              (std::vector<int64_t>{50, 40, 30, 20, 10}));
}

TEST(TopK, LimitSmallerThanInputReturnsTopK) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({30, 10, 50, 20, 40})));

    auto rg = RunTopK(Batches(std::move(batch)), Keys(Desc("x", kI64)), /*limit=*/3);

    EXPECT_EQ(Values<int64_t>(*rg, "x"), (std::vector<int64_t>{50, 40, 30}));
}

TEST(TopK, LimitLargerThanInputReturnsAllRows) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3})));

    auto rg = RunTopK(Batches(std::move(batch)), Keys(Asc("x", kI64)), /*limit=*/100);

    EXPECT_EQ(Values<int64_t>(*rg, "x"), (std::vector<int64_t>{1, 2, 3}));
}

TEST(TopK, OffsetZeroBehavesAsPlainLimit) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({30, 10, 50, 20, 40})));

    auto rg = RunTopK(Batches(std::move(batch)),
                      Keys(Desc("x", kI64)), /*limit=*/3, /*offset=*/0);

    EXPECT_EQ(Values<int64_t>(*rg, "x"), (std::vector<int64_t>{50, 40, 30}));
}

TEST(TopK, OffsetSkipsInitialRowsThenTakesLimit) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({30, 10, 50, 20, 40})));

    auto rg = RunTopK(Batches(std::move(batch)),
                      Keys(Desc("x", kI64)), /*limit=*/2, /*offset=*/1);

    EXPECT_EQ(Values<int64_t>(*rg, "x"), (std::vector<int64_t>{40, 30}))
        << "after sort 50,40,30,20,10 skip 1 → take 2 → [40, 30]";
}

TEST(TopK, OffsetEqualToInputSizeReturnsEmpty) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3})));

    auto rg = RunTopK(Batches(std::move(batch)),
                      Keys(Asc("x", kI64)), /*limit=*/10, /*offset=*/3);

    EXPECT_EQ(rg->GetRowCount(), 0u);
}

TEST(TopK, OffsetBeyondInputSizeReturnsEmpty) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3})));

    auto rg = RunTopK(Batches(std::move(batch)),
                      Keys(Asc("x", kI64)), /*limit=*/10, /*offset=*/100);

    EXPECT_EQ(rg->GetRowCount(), 0u);
}

TEST(TopK, OffsetPlusLimitExceedsInputReturnsRemainder) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3, 4, 5})));

    auto rg = RunTopK(Batches(std::move(batch)),
                      Keys(Asc("x", kI64)), /*limit=*/10, /*offset=*/3);

    EXPECT_EQ(Values<int64_t>(*rg, "x"), (std::vector<int64_t>{4, 5}));
}

TEST(TopK, MultiKeyBreaksTiesBySecondaryKey) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"p", kI64}, {"s", kI64}}),
        MakeColumn<int64_t>({1, 2, 1, 2, 1}),
        MakeColumn<int64_t>({5, 10, 3, 8, 7})));

    auto rg = RunTopK(Batches(std::move(batch)),
                      Keys(Asc("p", kI64), Desc("s", kI64)), /*limit=*/10);

    EXPECT_EQ(Values<int64_t>(*rg, "p"),
              (std::vector<int64_t>{1, 1, 1, 2, 2}));
    EXPECT_EQ(Values<int64_t>(*rg, "s"),
              (std::vector<int64_t>{7, 5, 3, 10, 8}))
        << "p ascending, then within p=1: 7,5,3 desc; within p=2: 10,8 desc";
}

TEST(TopK, MultipleBatchesAccumulateInTopK) {
    auto b1 = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({10, 50, 30})));
    auto b2 = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({40, 20, 60})));

    auto rg = RunTopK(Batches(std::move(b1), std::move(b2)),
                      Keys(Desc("x", kI64)), /*limit=*/3);

    EXPECT_EQ(Values<int64_t>(*rg, "x"), (std::vector<int64_t>{60, 50, 40}));
}

TEST(TopK, RespectsInputSelectionVector) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({100, 5, 200, 3, 150}));
    auto batch = MakeBatchWithSelection(rowGroup, {1, 3});

    auto rg = RunTopK(Batches(std::move(batch)),
                      Keys(Desc("x", kI64)), /*limit=*/10);

    EXPECT_EQ(Values<int64_t>(*rg, "x"), (std::vector<int64_t>{5, 3}))
        << "rows outside input selection (100, 200, 150) must be invisible";
}

TEST(TopK, StringKeySortsLexicographically) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"name", kStr}}),
        MakeColumn<std::string>({"banana", "apple", "cherry", "ant"})));

    auto rg = RunTopK(Batches(std::move(batch)),
                      Keys(Asc("name", kStr)), /*limit=*/10);

    EXPECT_EQ(Values<std::string>(*rg, "name"),
              (std::vector<std::string>{"ant", "apple", "banana", "cherry"}));
}

TEST(TopK, Int32KeyPreservesPhysicalType) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI32}}),
        MakeColumn<int32_t>({-10, 5, 0, -3, 100})));

    auto rg = RunTopK(Batches(std::move(batch)),
                      Keys(Asc("x", kI32)), /*limit=*/10);

    EXPECT_EQ(Values<int32_t>(*rg, "x"),
              (std::vector<int32_t>{-10, -3, 0, 5, 100}));
}

TEST(TopK, AllNonKeyColumnsPropagateToOutput) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}, {"label", kStr}, {"y", kI64}}),
        MakeColumn<int64_t>({30, 10, 20}),
        MakeColumn<std::string>({"c", "a", "b"}),
        MakeColumn<int64_t>({300, 100, 200})));

    auto rg = RunTopK(Batches(std::move(batch)),
                      Keys(Asc("x", kI64)), /*limit=*/10);

    EXPECT_EQ(Values<int64_t>(*rg, "x"), (std::vector<int64_t>{10, 20, 30}));
    EXPECT_EQ(Values<std::string>(*rg, "label"),
              (std::vector<std::string>{"a", "b", "c"}));
    EXPECT_EQ(Values<int64_t>(*rg, "y"), (std::vector<int64_t>{100, 200, 300}));
}

TEST(TopK, SecondNextAfterProducingReturnsFalse) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({1, 2, 3})));

    Exec::TopK op(std::make_unique<FakeOperator>(Batches(std::move(batch))),
                  Keys(Asc("x", kI64)), 10);
    op.Open();
    Exec::ExecBatch first;
    Exec::ExecBatch second;
    const bool firstOk = op.Next(first);
    const bool secondOk = op.Next(second);
    op.Close();

    EXPECT_TRUE(firstOk);
    EXPECT_FALSE(secondOk);
}

TEST(TopK, ReopenResetsAndProducesAgain) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}}),
        MakeColumn<int64_t>({3, 1, 2})));

    Exec::TopK op(std::make_unique<FakeOperator>(Batches(std::move(batch))),
                  Keys(Asc("x", kI64)), 10);

    op.Open();
    Exec::ExecBatch out;
    op.Next(out);
    op.Close();

    op.Open();
    Exec::ExecBatch outAgain;
    const bool ok = op.Next(outAgain);
    op.Close();

    EXPECT_TRUE(ok);
    EXPECT_EQ(Values<int64_t>(*outAgain.rowGroup, "x"),
              (std::vector<int64_t>{1, 2, 3}));
}

TEST(TopK, AllRowsEqualOnKeyKeepsLimitMany) {
    auto batch = MakeBatch(MakeRowGroupOf(
        MakeSchema({{"x", kI64}, {"id", kI64}}),
        MakeColumn<int64_t>({7, 7, 7, 7, 7}),
        MakeColumn<int64_t>({100, 200, 300, 400, 500})));

    auto rg = RunTopK(Batches(std::move(batch)),
                      Keys(Desc("x", kI64)), /*limit=*/3);

    EXPECT_EQ(rg->GetRowCount(), 3u)
        << "even when all keys tie, output respects LIMIT";
    EXPECT_EQ(Values<int64_t>(*rg, "x"),
              (std::vector<int64_t>{7, 7, 7}));
}

}  // namespace Columnar::Test
