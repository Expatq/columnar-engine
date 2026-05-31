#include <exec/aggregate/groupby/serializer.h>

#include <exec/expression/column_ref/column_ref.h>

#include <core/types.h>

#include <util/int128.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace Columnar::Test {

namespace {

constexpr auto kBool = Types::LogicalType::BOOL;
constexpr auto kI16 = Types::LogicalType::INT16;
constexpr auto kI32 = Types::LogicalType::INT32;
constexpr auto kI64 = Types::LogicalType::INT64;
constexpr auto kI128 = Types::LogicalType::INT128;
constexpr auto kStr = Types::LogicalType::STRING;

Exec::GroupByKey Key(std::string name, Types::LogicalType type) {
    return Exec::GroupByKey{
        std::make_unique<Exec::ColumnRefExpression>(std::string{name}, type),
        std::move(name)};
}

template <typename... Args>
std::vector<Exec::GroupByKey> Keys(Args&&... ks) {
    std::vector<Exec::GroupByKey> v;
    v.reserve(sizeof...(ks));
    (v.emplace_back(std::forward<Args>(ks)), ...);
    return v;
}

template <typename T>
Exec::ColumnSpan SpanOf(const std::vector<T>& v) {
    return Exec::ColumnSpan{std::span<const T>{v}};
}

}  // namespace

TEST(PackedSize, FixedTypesSumToTheirByteWidths) {
    EXPECT_EQ(Exec::GroupByKeySerializer::PackedSize(Keys(Key("b", kBool))), 1u);
    EXPECT_EQ(Exec::GroupByKeySerializer::PackedSize(Keys(Key("a", kI16))), 2u);
    EXPECT_EQ(Exec::GroupByKeySerializer::PackedSize(Keys(Key("a", kI32))), 4u);
    EXPECT_EQ(Exec::GroupByKeySerializer::PackedSize(Keys(Key("a", kI64))), 8u);
    EXPECT_EQ(Exec::GroupByKeySerializer::PackedSize(Keys(Key("a", kI128))), 16u);
}

TEST(PackedSize, MultipleFixedKeysAddUp) {
    EXPECT_EQ(
        Exec::GroupByKeySerializer::PackedSize(
            Keys(Key("a", kI64), Key("b", kI32))),
        12u);
}

TEST(PackedSize, StringMakesPackedSizeUnboundedSentinel) {
    EXPECT_EQ(
        Exec::GroupByKeySerializer::PackedSize(
            Keys(Key("s", kStr))),
        SIZE_MAX);
    EXPECT_EQ(
        Exec::GroupByKeySerializer::PackedSize(
            Keys(Key("i", kI64), Key("s", kStr))),
        SIZE_MAX);
}

TEST(PackInt64, SingleInt64KeyRoundTripsViaDeserializePacked) {
    const std::vector<int64_t> values{static_cast<int64_t>(0xDEADBEEFCAFEBABELL)};
    const std::vector<Exec::ColumnSpan> cols{SpanOf(values)};

    const uint64_t packed = Exec::GroupByKeySerializer::PackInt64(cols, 0);
    const auto unpacked = Exec::GroupByKeySerializer::DeserializePacked(
        &packed, Keys(Key("a", kI64)));

    ASSERT_EQ(unpacked.size(), 1u);
    EXPECT_EQ(std::get<int64_t>(unpacked[0]), static_cast<int64_t>(0xDEADBEEFCAFEBABELL));
}

TEST(PackInt64, CompositeIntKeysPackAndUnpack) {
    const std::vector<int32_t> a{42};
    const std::vector<int32_t> b{-1};
    const std::vector<Exec::ColumnSpan> cols{SpanOf(a), SpanOf(b)};

    const uint64_t packed = Exec::GroupByKeySerializer::PackInt64(cols, 0);
    const auto unpacked = Exec::GroupByKeySerializer::DeserializePacked(
        &packed, Keys(Key("a", kI32), Key("b", kI32)));

    ASSERT_EQ(unpacked.size(), 2u);
    EXPECT_EQ(std::get<int32_t>(unpacked[0]), 42);
    EXPECT_EQ(std::get<int32_t>(unpacked[1]), -1);
}

TEST(PackInt64, DifferentKeysProduceDifferentPackedValues) {
    const std::vector<int64_t> a{1};
    const std::vector<int64_t> b{2};

    const uint64_t packedA = Exec::GroupByKeySerializer::PackInt64({SpanOf(a)}, 0);
    const uint64_t packedB = Exec::GroupByKeySerializer::PackInt64({SpanOf(b)}, 0);

    EXPECT_NE(packedA, packedB);
}

TEST(PackInt128, RoundTripsInt128KeyPreservingFullWidth) {
    const Int128 big = Int128(1) << 100;
    const std::vector<Int128> values{big};

    const Int128 packed = Exec::GroupByKeySerializer::PackInt128({SpanOf(values)}, 0);
    const auto unpacked = Exec::GroupByKeySerializer::DeserializePacked(
        &packed, Keys(Key("x", kI128)));

    ASSERT_EQ(unpacked.size(), 1u);
    EXPECT_EQ(std::get<Int128>(unpacked[0]), big);
}

TEST(PackInt128, MixedFixedKeysRoundTrip) {
    const std::vector<int64_t> a{12345};
    const std::vector<int32_t> b{-99};
    const std::vector<int16_t> c{1000};

    const Int128 packed = Exec::GroupByKeySerializer::PackInt128(
        {SpanOf(a), SpanOf(b), SpanOf(c)}, 0);
    const auto unpacked = Exec::GroupByKeySerializer::DeserializePacked(
        &packed, Keys(Key("a", kI64), Key("b", kI32), Key("c", kI16)));

    ASSERT_EQ(unpacked.size(), 3u);
    EXPECT_EQ(std::get<int64_t>(unpacked[0]), 12345);
    EXPECT_EQ(std::get<int32_t>(unpacked[1]), -99);
    EXPECT_EQ(std::get<int16_t>(unpacked[2]), 1000);
}

TEST(Serialize, StringKeyEncodesLengthThenBytes) {
    const std::vector<std::string> values{"hello"};

    const auto buf = Exec::GroupByKeySerializer::Serialize({SpanOf(values)}, 0);

    ASSERT_EQ(buf.size(), sizeof(uint32_t) + 5);
    uint32_t lenStored;
    std::memcpy(&lenStored, buf.data(), sizeof(lenStored));
    EXPECT_EQ(lenStored, 5u);
    EXPECT_EQ(std::string_view(buf.data() + sizeof(uint32_t), 5), "hello");
}

TEST(Serialize, RoundTripsStringPlusIntsViaDeserializeInline) {
    const std::vector<std::string> name{"alice"};
    const std::vector<int64_t> id{12345};
    const std::vector<int32_t> age{-7};

    const auto buf = Exec::GroupByKeySerializer::Serialize(
        {SpanOf(name), SpanOf(id), SpanOf(age)}, 0);
    const auto values = Exec::GroupByKeySerializer::DeserializeInline(
        buf, Keys(Key("name", kStr), Key("id", kI64), Key("age", kI32)));

    ASSERT_EQ(values.size(), 3u);
    EXPECT_EQ(std::get<std::string>(values[0]), "alice");
    EXPECT_EQ(std::get<int64_t>(values[1]), 12345);
    EXPECT_EQ(std::get<int32_t>(values[2]), -7);
}

TEST(Serialize, EmptyStringSerializesAsFourZeroBytes) {
    const std::vector<std::string> values{""};

    const auto buf = Exec::GroupByKeySerializer::Serialize({SpanOf(values)}, 0);

    ASSERT_EQ(buf.size(), sizeof(uint32_t));
    uint32_t lenStored;
    std::memcpy(&lenStored, buf.data(), sizeof(lenStored));
    EXPECT_EQ(lenStored, 0u);
}

TEST(Serialize, MultipleStringKeysRoundTrip) {
    const std::vector<std::string> a{"foo"};
    const std::vector<std::string> b{"barbaz"};

    const auto buf = Exec::GroupByKeySerializer::Serialize(
        {SpanOf(a), SpanOf(b)}, 0);
    const auto values = Exec::GroupByKeySerializer::DeserializeInline(
        buf, Keys(Key("a", kStr), Key("b", kStr)));

    ASSERT_EQ(values.size(), 2u);
    EXPECT_EQ(std::get<std::string>(values[0]), "foo");
    EXPECT_EQ(std::get<std::string>(values[1]), "barbaz");
}

TEST(Serialize, Int128KeyInInlineFormatRoundTrips) {
    const Int128 v = (Int128(1) << 100) + Int128(7);
    const std::vector<Int128> values{v};

    const auto buf = Exec::GroupByKeySerializer::Serialize({SpanOf(values)}, 0);
    const auto unpacked = Exec::GroupByKeySerializer::DeserializeInline(
        buf, Keys(Key("x", kI128)));

    ASSERT_EQ(unpacked.size(), 1u);
    EXPECT_EQ(std::get<Int128>(unpacked[0]), v);
}

TEST(MakeInlineKey, ShortKeyStoresAllBytesInPrefixWithoutArenaPush) {
    Exec::KeysArena arena;
    const std::string data = "short";

    const Exec::InlineKey key = Exec::GroupByKeySerializer::MakeInlineKey(
        data.data(), data.size(), &arena);

    EXPECT_EQ(key.len, 5u);
    EXPECT_TRUE(key.IsInline());
    EXPECT_EQ(arena.pos, 0u);
    EXPECT_EQ(std::memcmp(key.prefix, "short", 5), 0);
}

TEST(MakeInlineKey, LongKeyCopiesPrefixAndPushesAllBytesIntoArena) {
    Exec::KeysArena arena;
    const std::string data(Exec::InlineKey::kPrefixBytes + 10, 'x');

    const Exec::InlineKey key = Exec::GroupByKeySerializer::MakeInlineKey(
        data.data(), data.size(), &arena);

    EXPECT_EQ(key.len, data.size());
    EXPECT_FALSE(key.IsInline());
    EXPECT_EQ(arena.pos, data.size());
    EXPECT_EQ(std::memcmp(arena.Data() + key.arenaOffset, data.data(), data.size()), 0);
}

TEST(MakeInlineKey, EqualContentProducesEqualHashAcrossCalls) {
    Exec::KeysArena arena;
    const std::string data = "consistent";

    const Exec::InlineKey a = Exec::GroupByKeySerializer::MakeInlineKey(
        data.data(), data.size(), &arena);
    const Exec::InlineKey b = Exec::GroupByKeySerializer::MakeInlineKey(
        data.data(), data.size(), &arena);

    EXPECT_EQ(a.hash, b.hash);
    EXPECT_EQ(a.len, b.len);
}

}  // namespace Columnar::Test
