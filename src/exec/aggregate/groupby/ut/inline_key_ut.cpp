#include <exec/aggregate/groupby/inline_key.h>

#include <util/int128.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>

namespace Columnar::Test {

namespace {

Exec::InlineKey MakeKey(uint32_t hash, const std::string& data, Exec::KeysArena& arena) {
    Exec::InlineKey key;
    key.hash = hash;
    key.len = static_cast<uint32_t>(data.size());
    const size_t inPrefix = std::min<size_t>(data.size(), Exec::InlineKey::kPrefixBytes);
    std::memcpy(key.prefix, data.data(), inPrefix);
    if (data.size() > Exec::InlineKey::kPrefixBytes) {
        key.arenaOffset = arena.Push(data.data(), data.size());
    }
    return key;
}

}  // namespace

TEST(KeysArena, PushReturnsIncrementingOffsets) {
    Exec::KeysArena arena;

    const uint32_t off1 = arena.Push("foo", 3);
    const uint32_t off2 = arena.Push("bar", 3);
    const uint32_t off3 = arena.Push("", 0);

    EXPECT_EQ(off1, 0u);
    EXPECT_EQ(off2, 3u);
    EXPECT_EQ(off3, 6u);
}

TEST(KeysArena, GrowsCapacityToFitLargeChunkInOneShot) {
    Exec::KeysArena arena;
    const std::string big(1024, 'x');

    const uint32_t off = arena.Push(big.data(), big.size());

    EXPECT_EQ(off, 0u);
    EXPECT_EQ(std::memcmp(arena.Data(), big.data(), big.size()), 0);
}

TEST(KeysArena, DoublesCapacityOnRepeatedPushes) {
    Exec::KeysArena arena;
    std::string accumulated;

    for (int i = 0; i < 100; ++i) {
        const std::string chunk(64, static_cast<char>('a' + (i % 26)));
        arena.Push(chunk.data(), chunk.size());
        accumulated += chunk;
    }

    EXPECT_EQ(std::memcmp(arena.Data(), accumulated.data(), accumulated.size()), 0);
}

TEST(KeysArena, ResetReusesBufferWithoutLosingPreviousData) {
    Exec::KeysArena arena;
    arena.Push("hello", 5);
    arena.Reset();

    const uint32_t off = arena.Push("world", 5);

    EXPECT_EQ(off, 0u) << "Reset restarts offsets from zero";
    EXPECT_EQ(std::memcmp(arena.Data(), "world", 5), 0);
}

TEST(InlineKey, ZeroLenKeyIsInline) {
    Exec::InlineKey key;

    EXPECT_TRUE(key.IsInline());
}

TEST(InlineKey, LenEqualToPrefixSizeIsInline) {
    Exec::InlineKey key;
    key.len = Exec::InlineKey::kPrefixBytes;

    EXPECT_TRUE(key.IsInline()) << "exactly kPrefixBytes still fits in prefix";
}

TEST(InlineKey, LenAbovePrefixSizeRequiresArena) {
    Exec::InlineKey key;
    key.len = Exec::InlineKey::kPrefixBytes + 1;

    EXPECT_FALSE(key.IsInline());
}

TEST(InlineKeyHash, ReturnsStoredHashField) {
    Exec::InlineKey key;
    key.hash = 0xDEADBEEF;

    EXPECT_EQ(Exec::InlineKeyHash{}(key), 0xDEADBEEFu);
}

TEST(InlineKeyEq, DifferentHashShortCircuitsToFalse) {
    Exec::KeysArena arena;
    Exec::InlineKey a = MakeKey(1, "foo", arena);
    Exec::InlineKey b = MakeKey(2, "foo", arena);

    EXPECT_FALSE((Exec::InlineKeyEq{&arena})(a, b));
}

TEST(InlineKeyEq, DifferentLenShortCircuitsToFalse) {
    Exec::KeysArena arena;
    Exec::InlineKey a = MakeKey(7, "foo", arena);
    Exec::InlineKey b = MakeKey(7, "foobar", arena);

    EXPECT_FALSE((Exec::InlineKeyEq{&arena})(a, b));
}

TEST(InlineKeyEq, SameHashSameLenDifferentPrefixIsNotEqual) {
    Exec::KeysArena arena;
    Exec::InlineKey a = MakeKey(7, "abc", arena);
    Exec::InlineKey b = MakeKey(7, "xyz", arena);

    EXPECT_FALSE((Exec::InlineKeyEq{&arena})(a, b))
        << "hash collision must not produce false positive when prefixes differ";
}

TEST(InlineKeyEq, EqualShortKeysCompareEqual) {
    Exec::KeysArena arena;
    Exec::InlineKey a = MakeKey(7, "hello", arena);
    Exec::InlineKey b = MakeKey(7, "hello", arena);

    EXPECT_TRUE((Exec::InlineKeyEq{&arena})(a, b));
}

TEST(InlineKeyEq, BoundaryLengthExactlyPrefixBytesUsesPrefixOnly) {
    Exec::KeysArena arena;
    const std::string exact(Exec::InlineKey::kPrefixBytes, 'a');
    Exec::InlineKey a = MakeKey(42, exact, arena);
    Exec::InlineKey b = MakeKey(42, exact, arena);

    EXPECT_TRUE((Exec::InlineKeyEq{&arena})(a, b));
    EXPECT_EQ(a.arenaOffset, 0u) << "no arena push expected for len == kPrefixBytes";
}

TEST(InlineKeyEq, LongKeysWithSameContentAreEqual) {
    Exec::KeysArena arena;
    const std::string longStr(Exec::InlineKey::kPrefixBytes + 50, 'q');
    Exec::InlineKey a = MakeKey(42, longStr, arena);
    Exec::InlineKey b = MakeKey(42, longStr, arena);

    EXPECT_TRUE((Exec::InlineKeyEq{&arena})(a, b));
    EXPECT_NE(a.arenaOffset, b.arenaOffset)
        << "two separate Push calls write to distinct offsets even for equal content";
}

TEST(InlineKeyEq, LongKeysSamePrefixDifferentSuffixAreNotEqual) {
    Exec::KeysArena arena;
    std::string s1(Exec::InlineKey::kPrefixBytes + 10, 'a');
    std::string s2(Exec::InlineKey::kPrefixBytes + 10, 'a');
    s1.back() = 'X';
    s2.back() = 'Y';

    Exec::InlineKey a = MakeKey(7, s1, arena);
    Exec::InlineKey b = MakeKey(7, s2, arena);

    EXPECT_FALSE((Exec::InlineKeyEq{&arena})(a, b))
        << "prefix matches but arena content differs at the tail; eq must walk full length";
}

TEST(Int128Hash, IsDeterministicForSameValue) {
    const Int128 v = Int128(1) << 100;

    EXPECT_EQ(Exec::Int128Hash{}(v), Exec::Int128Hash{}(v));
}

TEST(Int128Hash, DistinguishesDifferentLowHalves) {
    const Int128 a = Int128(1);
    const Int128 b = Int128(2);

    EXPECT_NE(Exec::Int128Hash{}(a), Exec::Int128Hash{}(b));
}

TEST(Int128Hash, DistinguishesDifferentHighHalves) {
    const Int128 a = Int128(1) << 64;
    const Int128 b = Int128(2) << 64;

    EXPECT_NE(Exec::Int128Hash{}(a), Exec::Int128Hash{}(b));
}

TEST(Int128Hash, ValueZeroProducesZeroHash) {
    EXPECT_EQ(Exec::Int128Hash{}(Int128(0)), 0u);
}

TEST(Int128Hash, NegativeAndPositiveSameMagnitudeDifferInHash) {
    const Int128 pos = Int128(123456789);
    const Int128 neg = -Int128(123456789);

    EXPECT_NE(Exec::Int128Hash{}(pos), Exec::Int128Hash{}(neg));
}

}  // namespace Columnar::Test
