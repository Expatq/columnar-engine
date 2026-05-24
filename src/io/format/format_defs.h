#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>

namespace Columnar::IO {

constexpr uint8_t kMagicBytes[4] = {'I', 'Y', 'X', 0x01};
constexpr size_t kMagicSize = 4;

// IYX header (64 bytes):
//   [0..3]   colCount       uint32
//   [4..7]   rgCount        uint32
//   [8..15]  totalRowCount  uint64
//   [16..23] schemaOffset   uint64
//   [24..31] footerOffset   uint64
//   [32..63] reserved       uint8[32]

constexpr size_t kHeaderSize = 64;
constexpr size_t kHeaderOffsetRgCount = 4;
constexpr size_t kHeaderOffsetFooterOffset = 24;
constexpr size_t kHeaderReservedSize = 32;

constexpr uint8_t kEncRaw = 0x00;      // values stored as-is
constexpr uint8_t kEncBitpack = 0x01;  // delta from min + bit-packing

// Bit-packed column header (after enc byte):
// int64 minVal + uint8 bitsNeeded
constexpr size_t kBitpackHeaderBytes = sizeof(int64_t) + sizeof(uint8_t);

// Use bit-packing when it saves at least 25% space.
// Condition: bits_needed < raw_bits × 3/4.
// Written as multiplication to avoid integer division:
//   bits_needed × 4 < sizeof(T) × CHAR_BIT × 3
template <typename T>
constexpr bool ShouldBitpack(uint8_t bitsNeeded) noexcept {
    return static_cast<unsigned>(bitsNeeded) * 4 < sizeof(T) * static_cast<unsigned>(CHAR_BIT) * 3;
}

constexpr size_t kDefaultRowGroupSize = 1u << 16;  // 65 536
constexpr size_t kStringAvgEstimatedBytes = 64;

}  // namespace Columnar::IO
