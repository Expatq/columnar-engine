#include "csv_boundary_index.h"

#include <io/binary/file_ops.h>

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace Columnar::IO {

namespace {

class UniqueFd {
public:
    explicit UniqueFd(const std::filesystem::path& path) {
        fd_ = ::open(path.c_str(), O_RDONLY);
        if (fd_ < 0) {
            throw std::runtime_error("open failed for " + path.string() + ": " +
                                     std::strerror(errno));
        }
    }

    ~UniqueFd() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    int Get() const {
        return fd_;
    }

private:
    int fd_ = -1;
};

size_t ResolveThreads(size_t requested) {
    if (requested != 0) {
        return requested;
    }

    const unsigned hw = std::thread::hardware_concurrency();
    return hw == 0 ? 4 : static_cast<size_t>(hw);
}

}  // namespace

CsvBoundaryIndexer::PartitionPlan CsvBoundaryIndexer::BuildPlan(
    const std::filesystem::path& csvPath,
    const Options& options) const {
    UniqueFd fd(csvPath);

    const uint64_t fileSize = GetFileSize(fd.Get());
    if (fileSize == 0) {
        throw std::runtime_error("CSV file is empty: " + csvPath.string());
    }

    const size_t threads = ResolveThreads(options.threads);
    const size_t blockSize = std::max<size_t>(options.blockSize, 4096);
    const auto blocks = MakeBlocks(fileSize, blockSize);

    std::vector<BlockSummary> summaries(blocks.size());
    std::atomic<size_t> nextBlock = 0;

    std::mutex errorMutex;
    std::exception_ptr firstError;

    auto saveError = [&](std::exception_ptr error) {
        std::lock_guard guard(errorMutex);
        if (!firstError) {
            firstError = error;
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(threads);

    for (size_t i = 0; i < threads; ++i) {
        workers.emplace_back([&] {
            try {
                while (true) {
                    const size_t blockIdx = nextBlock.fetch_add(1, std::memory_order_relaxed);
                    if (blockIdx >= blocks.size()) {
                        return;
                    }

                    summaries[blockIdx] = ScanBlock(fd.Get(),
                                                    blocks[blockIdx].first,
                                                    blocks[blockIdx].second);
                }
            } catch (...) {
                saveError(std::current_exception());
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    if (firstError) {
        std::rethrow_exception(firstError);
    }

    size_t recordEndCount = 0;
    CsvState state = CsvState::Outside;
    for (const auto& summary : summaries) {
        const auto stateIdx = static_cast<size_t>(state);
        recordEndCount += summary.newLines[stateIdx].size();
        state = summary.endState[stateIdx];
    }

    if (state == CsvState::InQuoted) {
        throw std::runtime_error("CSV has an unclosed quoted record");
    }

    std::vector<uint64_t> recordEnds;
    recordEnds.reserve(recordEndCount + 1);

    state = CsvState::Outside;
    for (const auto& summary : summaries) {
        const auto stateIdx = static_cast<size_t>(state);
        recordEnds.insert(recordEnds.end(),
                          summary.newLines[stateIdx].begin(),
                          summary.newLines[stateIdx].end());
        state = summary.endState[stateIdx];
    }

    if (recordEnds.empty() || recordEnds.back() + 1 != fileSize) {
        recordEnds.push_back(fileSize - 1);
    }

    PartitionPlan plan;
    plan.fileSize = fileSize;
    plan.recordCount = recordEnds.size();

    const size_t targetPartitions =
        options.targetPartitions != 0 ? options.targetPartitions : threads * 4;
    const uint64_t targetBytes =
        std::max<uint64_t>(1, (fileSize + targetPartitions - 1) / targetPartitions);
    const uint64_t targetRows =
        std::max<uint64_t>(1, (plan.recordCount + targetPartitions - 1) / targetPartitions);

    uint64_t rangeBegin = 0;
    uint64_t firstRow = 0;
    uint64_t rowsInRange = 0;

    for (uint64_t row = 0; row < plan.recordCount; ++row) {
        ++rowsInRange;

        const uint64_t recordEnd = recordEnds[row] + 1;
        const bool shouldCut =
            recordEnd - rangeBegin >= targetBytes || rowsInRange >= targetRows;

        if (shouldCut && row + 1 < plan.recordCount) {
            plan.ranges.push_back(RecordRange{
                .beginOffset = rangeBegin,
                .endOffset = recordEnd,
                .firstRow = firstRow,
                .rowCount = rowsInRange,
            });

            rangeBegin = recordEnd;
            firstRow = row + 1;
            rowsInRange = 0;
        }
    }

    if (rowsInRange != 0) {
        plan.ranges.push_back(RecordRange{
            .beginOffset = rangeBegin,
            .endOffset = fileSize,
            .firstRow = firstRow,
            .rowCount = rowsInRange,
        });
    }

    return plan;
}

CsvBoundaryIndexer::CsvState CsvBoundaryIndexer::ProcessOutside(
    char ch,
    uint64_t offset,
    std::vector<uint64_t>& newLines) {
    if (ch == '"') {
        return CsvState::InQuoted;
    }
    if (ch == '\n') {
        newLines.push_back(offset);
    }
    return CsvState::Outside;
}

void CsvBoundaryIndexer::ScanBytes(const std::vector<char>& bytes,
                                   uint64_t globalBegin,
                                   CsvState startState,
                                   CsvState* endState,
                                   std::vector<uint64_t>* newLines) {
    CsvState state = startState;

    for (size_t i = 0; i < bytes.size(); ++i) {
        const char ch = bytes[i];
        switch (state) {
            case CsvState::Outside:
                state = ProcessOutside(ch, globalBegin + i, *newLines);
                break;
            case CsvState::InQuoted:
                if (ch == '"') {
                    state = CsvState::AfterQuoteInQuoted;
                }
                break;
            case CsvState::AfterQuoteInQuoted:
                if (ch == '"') {
                    state = CsvState::InQuoted;
                } else {
                    state = ProcessOutside(ch, globalBegin + i, *newLines);
                }
                break;
        }
    }

    *endState = state;
}

std::vector<std::pair<uint64_t, uint64_t>> CsvBoundaryIndexer::MakeBlocks(
    uint64_t fileSize,
    size_t blockSize) {
    std::vector<std::pair<uint64_t, uint64_t>> blocks;
    for (uint64_t begin = 0; begin < fileSize; begin += blockSize) {
        const uint64_t end = std::min<uint64_t>(fileSize, begin + blockSize);
        blocks.emplace_back(begin, end);
    }
    return blocks;
}

CsvBoundaryIndexer::BlockSummary CsvBoundaryIndexer::ScanBlock(
    int fd,
    uint64_t begin,
    uint64_t end) {
    BlockSummary summary;
    summary.begin = begin;
    summary.end = end;

    std::vector<char> bytes(static_cast<size_t>(end - begin));
    if (!bytes.empty()) {
        ReadAll(fd, bytes.data(), bytes.size(), begin);
    }

    for (size_t state = 0; state < 3; ++state) {
        ScanBytes(bytes,
                  begin,
                  static_cast<CsvState>(state),
                  &summary.endState[state],
                  &summary.newLines[state]);
    }

    return summary;
}

}  // namespace Columnar::IO
