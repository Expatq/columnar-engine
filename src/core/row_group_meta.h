#pragma once

#include "col_stats.h"

#include <absl/container/inlined_vector.h>

#include <cstdint>

namespace Columnar::IO {

struct RowGroupMeta {
    uint64_t offset   = 0;
    uint32_t rowCount = 0;
    absl::InlinedVector<ColStats, 8> colStats{};

    bool HasColStats() const {
        return !colStats.empty();
    }

    const ColStats& GetColStats(size_t colIdx) const {
        return colStats[colIdx];
    }
};

}  // namespace Columnar::IO
