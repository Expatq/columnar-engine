#pragma once

#include <cstdint>

namespace Columnar::IO {

struct RowGroupMeta {
    uint64_t offset = 0;
    uint32_t rowCount = 0;
};

}  // namespace Columnar::IO
