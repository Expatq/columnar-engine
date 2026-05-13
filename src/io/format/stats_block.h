#pragma once

#include <core/col_stats.h>

#include <vector>

namespace Columnar::IO {

struct StatsBlock {
    std::vector<std::vector<ColStats>> data;

    bool empty() const {
        return data.empty();
    }
};

}  // namespace Columnar::IO
