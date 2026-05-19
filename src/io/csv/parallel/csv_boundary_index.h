#pragma once

#include <util/size_literals.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <utility>
#include <vector>

namespace Columnar::IO {

class CsvBoundaryIndexer {
public:
    struct RecordRange {
        uint64_t beginOffset = 0;
        uint64_t endOffset = 0;
        uint64_t firstRow = 0;
        uint64_t rowCount = 0;
    };

    struct PartitionPlan {
        uint64_t fileSize = 0;
        uint64_t recordCount = 0;
        std::vector<RecordRange> ranges;
    };

    struct Options {
        size_t threads = 0;
        size_t blockSize = 16_MB;
        size_t targetPartitions = 0;
    };

public:
    PartitionPlan BuildPlan(const std::filesystem::path& csvPath, const Options& options) const;

private:
    enum class CsvState : uint8_t {
        Outside = 0,
        InQuoted = 1,
        AfterQuoteInQuoted = 2,
    };

    struct BlockSummary {
        uint64_t begin = 0;
        uint64_t end = 0;
        CsvState endState[3] = {};
        std::vector<uint64_t> newLines[3];
    };

private:
    static CsvState ProcessOutside(char ch, uint64_t offset, std::vector<uint64_t>& newLines);

    static void ScanBytes(const std::vector<char>& bytes,
                          uint64_t globalBegin,
                          CsvState startState,
                          CsvState* endState,
                          std::vector<uint64_t>* newLines);

    static std::vector<std::pair<uint64_t, uint64_t>> MakeBlocks(uint64_t fileSize, size_t blockSize);

    static BlockSummary ScanBlock(int fd, uint64_t begin, uint64_t end);
};

}  // namespace Columnar::IO
