#pragma once

#include <cstdint>
#include <limits>

namespace Columnar {

struct ColStats {
    static constexpr int64_t kNoMin = std::numeric_limits<int64_t>::min();
    static constexpr int64_t kNoMax = std::numeric_limits<int64_t>::max();

    int64_t minVal = kNoMin;
    int64_t maxVal = kNoMax;
    uint64_t nullCount = 0;  // TODO: support nulls

    bool MayContain(int64_t lo, int64_t hi) const {
        return maxVal >= lo && minVal <= hi;
    }

    bool MayEqual(int64_t value) const {
        return MayContain(value, value);
    }
};

}  // namespace Columnar
