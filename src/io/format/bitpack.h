#pragma once

#include <cstddef>
#include <cstdint>

namespace Columnar::IO {

inline uint8_t MinBits(uint64_t v) {
    return v == 0 ? 1u : static_cast<uint8_t>(64 - __builtin_clzll(v));
}

void BitpackEncode(const uint32_t* values, size_t n, uint8_t bitWidth, uint8_t* dst);

void BitpackDecodeI32(const uint8_t* src, size_t n, uint8_t bitWidth,
                      int64_t minVal, int32_t* dst);

void BitpackDecodeI16(const uint8_t* src, size_t n, uint8_t bitWidth,
                      int64_t minVal, int16_t* dst);

}  // namespace Columnar::IO
