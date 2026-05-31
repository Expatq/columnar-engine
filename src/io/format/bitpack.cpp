#include <io/format/bitpack.h>

#include <climits>
#include <cstdint>
#include <vector>

namespace Columnar::IO {

void BitpackEncode(const uint32_t* values, size_t n, uint8_t bitWidth, uint8_t* dst) {
    uint64_t buf = 0;
    int used = 0;
    size_t bi = 0;

    for (size_t i = 0; i < n; ++i) {
        buf |= static_cast<uint64_t>(values[i]) << used;
        used += bitWidth;
        while (used >= CHAR_BIT) {
            dst[bi++] = static_cast<uint8_t>(buf);
            buf >>= CHAR_BIT;
            used -= CHAR_BIT;
        }
    }

    if (used > 0)
        dst[bi] = static_cast<uint8_t>(buf);
}

static void DecodeI32(const uint8_t* src, size_t n, uint8_t bitWidth,
                      int64_t minVal, int32_t* dst) {
    constexpr unsigned kUInt32Bits = sizeof(uint32_t) * CHAR_BIT;
    const uint32_t mask = (bitWidth == kUInt32Bits) ? ~0u : ((1u << bitWidth) - 1u);
    uint64_t buf = 0;
    int bits = 0;
    size_t bi = 0;

    for (size_t i = 0; i < n; ++i) {
        while (bits < bitWidth) {
            buf |= static_cast<uint64_t>(src[bi++]) << bits;
            bits += CHAR_BIT;
        }
        dst[i] = static_cast<int32_t>(static_cast<int64_t>(buf & mask) + minVal);
        buf >>= bitWidth;
        bits -= bitWidth;
    }
}

// uint8 → int32 widen + add. Compiles to vpmovzxbd + vpaddd with -O3 -march=native.
static void DecodeI32_8(const uint8_t* src, size_t n, int64_t minVal, int32_t* dst) {
    const auto base = static_cast<int32_t>(minVal);
    for (size_t i = 0; i < n; ++i)
        dst[i] = static_cast<int32_t>(src[i]) + base;
}

// uint16 → int32 widen + add. Compiles to vpmovzxwd + vpaddd with -O3 -march=native.
static void DecodeI32_16(const uint8_t* src, size_t n, int64_t minVal, int32_t* dst) {
    const auto base = static_cast<int32_t>(minVal);
    const auto* src16 = reinterpret_cast<const uint16_t*>(src);
    for (size_t i = 0; i < n; ++i)
        dst[i] = static_cast<int32_t>(src16[i]) + base;
}

void BitpackDecodeI32(const uint8_t* src, size_t n, uint8_t bitWidth,
                      int64_t minVal, int32_t* dst) {
    if (bitWidth == 8)
        return DecodeI32_8(src, n, minVal, dst);
    if (bitWidth == 16)
        return DecodeI32_16(src, n, minVal, dst);
    DecodeI32(src, n, bitWidth, minVal, dst);
}

void BitpackDecodeI16(const uint8_t* src, size_t n, uint8_t bitWidth,
                      int64_t minVal, int16_t* dst) {
    std::vector<int32_t> tmp(n);
    BitpackDecodeI32(src, n, bitWidth, minVal, tmp.data());
    for (size_t i = 0; i < n; ++i)
        dst[i] = static_cast<int16_t>(tmp[i]);
}

}  // namespace Columnar::IO
