#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>

namespace Columnar::IO {

inline uint8_t MinBits(uint64_t v) {
    return v == 0 ? 1u
                  : static_cast<uint8_t>(sizeof(uint64_t) * CHAR_BIT - __builtin_clzll(v));
}

// Number of bytes needed to store valueCount values at bitsPerValue bits each.
// Ceiling division: (n * b + CHAR_BIT - 1) / CHAR_BIT.
constexpr size_t PackedBytes(size_t valueCount, uint8_t bitsPerValue) noexcept {
    return (valueCount * static_cast<size_t>(bitsPerValue) + CHAR_BIT - 1) / CHAR_BIT;
}

void BitpackEncode(const uint32_t* values, size_t n, uint8_t bitWidth, uint8_t* dst);

void BitpackDecodeI32(const uint8_t* src, size_t n, uint8_t bitWidth,
                      int64_t minVal, int32_t* dst);

void BitpackDecodeI16(const uint8_t* src, size_t n, uint8_t bitWidth,
                      int64_t minVal, int16_t* dst);

}  // namespace Columnar::IO
