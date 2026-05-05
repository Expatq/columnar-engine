#pragma once

#include "col_stats.h"

#include <cstdint>
#include <vector>

namespace Columnar::IO {

struct RowGroupMeta {
    uint64_t offset = 0;
    uint32_t rowCount = 0;
    std::vector<ColStats> colStats{};

    bool HasColStats() const {
        return !colStats.empty();
    }

    const ColStats& GetColStats(size_t colIdx) const {
        return colStats[colIdx];
    }
};

}  // namespace Columnar::IO
