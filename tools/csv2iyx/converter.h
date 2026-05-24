#pragma once

#include <io/format/format_defs.h>

#include <cstddef>
#include <string>

namespace Columnar {

struct ConvertOptions {
    std::string schemaPath;
    std::string csvPath;
    std::string iyxPath;
    size_t numThreads   = 0;                        // 0 → hardware_concurrency()
    size_t rowGroupSize = IO::kDefaultRowGroupSize;
};

void Run(const ConvertOptions& opts);

}  // namespace Columnar
