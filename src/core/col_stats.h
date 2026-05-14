#pragma once

#include <util/int128.h>

#include <cstdint>

namespace Columnar {

static constexpr Int128 kInt128Max = (Int128)(((UInt128) ~(UInt128)0) >> 1);
static constexpr Int128 kInt128Min = -kInt128Max - 1;

struct ColStats {
    Int128 minVal = kInt128Max;
    Int128 maxVal = kInt128Min;
    uint64_t nullCount = 0; // TODO: support null type

    bool MayContain(int64_t lo, int64_t hi) const {
        return static_cast<Int128>(lo) <= maxVal && static_cast<Int128>(hi) >= minVal;
    }

    bool MayEqual(int64_t value) const {
        return MayContain(value, value);
    }
};

}  // namespace Columnar
