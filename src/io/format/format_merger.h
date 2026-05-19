#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace Columnar::IO {

struct MergeInput {
    std::filesystem::path path;
};

struct MergeStats {
    uint64_t rows = 0;
    uint64_t rowGroups = 0;
    uint64_t bytesCopied = 0;
};

MergeStats MergeIyxFiles(const std::vector<MergeInput>& inputs,
                         const std::filesystem::path& output);

}  // namespace Columnar::IO
