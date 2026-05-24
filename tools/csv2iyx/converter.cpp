#include "converter.h"

#include <io/format/format_writer.h>
#include <io/format/lib/mmap_file.h>
#include <parser/csv/csv_parser.h>
#include <parser/format/schema_parser.h>
#include <parser/format/value_parser.h>

#include <core/column.h>
#include <core/row_group.h>
#include <core/schema.h>
#include <core/types.h>
#include <util/int128.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <exception>
#include <functional>
#include <mutex>
#include <queue>
#include <semaphore>
#include <stdexcept>
#include <thread>
#include <vector>

namespace Columnar {

namespace {

// Over-partition relative to thread count so a slow chunk doesn't stall others.
constexpr size_t kChunksPerThread   = 8;
// Limits in-flight RowGroup objects to bound memory usage.
// At 8 threads with hits.csv: 16 × 160 MB ≈ 2.5 GB peak.
constexpr size_t kMaxInFlightFactor = 2;

// ─── File splitting ────────────────────────────────────────────────────────────

// Each chunk ends exactly on '\n' so workers only see complete lines.
// Assumption: '\n' does not appear inside quoted fields (holds for hits.csv).
std::vector<std::pair<size_t, size_t>> SplitFile(
    const uint8_t* data, size_t fileSize, size_t numChunks)
{
    std::vector<std::pair<size_t, size_t>> result;
    result.reserve(numChunks);

    const size_t targetSize = fileSize / numChunks;
    size_t start = 0;

    for (size_t i = 0; i < numChunks; ++i) {
        if (i == numChunks - 1 || start >= fileSize) {
            if (start < fileSize)
                result.push_back({start, fileSize - start});
            break;
        }
        size_t end = std::min(start + targetSize, fileSize);
        while (end < fileSize && data[end] != '\n') ++end;
        if (end < fileSize) ++end;
        result.push_back({start, end - start});
        start = end;
    }

    return result;
}

// ─── Pipeline element ─────────────────────────────────────────────────────────

// Two-level tag for restoring write order from parallel workers.
// isLastInChunk signals the main thread that a chunk is fully consumed.
struct TaggedRowGroup {
    size_t   chunkIdx;
    size_t   rgIdx;
    bool     isLastInChunk;
    RowGroup rg;

    bool operator>(const TaggedRowGroup& o) const {
        if (chunkIdx != o.chunkIdx) return chunkIdx > o.chunkIdx;
        return rgIdx > o.rgIdx;
    }
};

// ─── Parsing ──────────────────────────────────────────────────────────────────

void AppendParsedToBuffer(Types::AnyColumnData& buf, const Types::AnyPhysicalType& val) {
    std::visit(
        Types::overloaded{
            [&](std::vector<int16_t>& v)     { v.push_back(std::get<int16_t>(val)); },
            [&](std::vector<int32_t>& v)     { v.push_back(std::get<int32_t>(val)); },
            [&](std::vector<int64_t>& v)     { v.push_back(std::get<int64_t>(val)); },
            [&](std::vector<uint8_t>& v)     { v.push_back(std::get<uint8_t>(val)); },
            [&](std::vector<std::string>& v) { v.push_back(std::get<std::string>(val)); },
            [&](std::vector<Int128>& v)      { v.push_back(std::get<Int128>(val)); },
        },
        buf);
}

// Parses a byte range of CSV, emitting each completed RowGroup immediately via
// callback. Workers hold no shared mutable state beyond the queue/semaphore
// accessed through the callback.
// The callback acquires a semaphore slot and may block when the queue is full.
void ParseChunk(
    const uint8_t* data, size_t offset, size_t length,
    const Schema& schema, size_t chunkIdx, size_t rowGroupSize,
    std::function<void(TaggedRowGroup)> emit)
{
    const size_t colCount = schema.GetColumnCount();

    std::vector<Types::AnyColumnData> bufs;
    bufs.reserve(colCount);
    for (size_t i = 0; i < colCount; ++i) {
        bufs.push_back(Types::CreateEmptyColumnData(schema.GetColumn(i).physical));
        std::visit([&](auto& v) { v.reserve(rowGroupSize); }, bufs.back());
    }

    size_t rowsInRg = 0;
    size_t rgIdx    = 0;

    auto flushRg = [&](bool isLast) {
        std::vector<Column> cols;
        cols.reserve(colCount);
        for (size_t i = 0; i < colCount; ++i) {
            cols.emplace_back(std::move(bufs[i]), schema.GetColumn(i).physical);
            bufs[i] = Types::CreateEmptyColumnData(schema.GetColumn(i).physical);
            std::visit([&](auto& v) { v.reserve(rowGroupSize); }, bufs[i]);
        }
        emit({chunkIdx, rgIdx++, isLast, RowGroup(schema, std::move(cols))});
        rowsInRg = 0;
    };

    const char* ptr = reinterpret_cast<const char*>(data + offset);
    const char* end = ptr + length;

    while (ptr < end) {
        const char* nl      = static_cast<const char*>(std::memchr(ptr, '\n', end - ptr));
        const char* lineEnd = nl ? nl : end;

        std::string_view line(ptr, lineEnd - ptr);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        ptr = lineEnd + (nl ? 1 : 0);

        if (line.empty()) continue;

        auto fields = Parser::ParseCsvLine(std::string(line));
        if (fields.size() != colCount)
            throw std::runtime_error(
                "field count mismatch in chunk " + std::to_string(chunkIdx) +
                ": expected " + std::to_string(colCount) +
                ", got " + std::to_string(fields.size()));

        for (size_t i = 0; i < colCount; ++i)
            AppendParsedToBuffer(
                bufs[i],
                Parser::ParseValue(fields[i], schema.GetColumn(i).logical));

        ++rowsInRg;
        if (rowsInRg >= rowGroupSize) flushRg(/*isLast=*/false);
    }

    // Last (possibly partial) row group, or an empty terminator for empty chunks.
    // isLastInChunk=true tells the main thread this chunk is complete.
    flushRg(/*isLast=*/true);
}

}  // namespace

// ─── Public entry point ────────────────────────────────────────────────────────

void Run(const ConvertOptions& opts) {
    const Schema schema = Parser::LoadSchemaFromCsv(opts.schemaPath);

    const size_t numThreads = opts.numThreads > 0
        ? opts.numThreads
        : std::thread::hardware_concurrency();

    IO::MmapFile csvFile(opts.csvPath);
    const uint8_t* data     = csvFile.Ptr(0);
    const size_t   fileSize = csvFile.GetFileSize();

    const size_t numChunks   = numThreads * kChunksPerThread;
    const size_t maxInFlight = numThreads * kMaxInFlightFactor;

    auto chunks = SplitFile(data, fileSize, numChunks);

    // Semaphore counts TaggedRowGroup objects in the queue.
    // A worker acquires a slot before emit; the main thread releases after AppendBlob.
    std::counting_semaphore<> semaphore(static_cast<std::ptrdiff_t>(maxInFlight));

    std::atomic<size_t> nextChunk{0};
    std::mutex          mtx;
    std::condition_variable cv;
    std::exception_ptr  firstError;

    using Heap = std::priority_queue<
        TaggedRowGroup, std::vector<TaggedRowGroup>, std::greater<TaggedRowGroup>>;
    Heap ready;

    std::vector<std::jthread> workers;
    workers.reserve(numThreads);

    for (size_t t = 0; t < numThreads; ++t) {
        workers.emplace_back([&](std::stop_token stoken) {
            while (!stoken.stop_requested()) {
                const size_t idx = nextChunk.fetch_add(1, std::memory_order_relaxed);
                if (idx >= chunks.size()) break;

                auto emit = [&](TaggedRowGroup trg) {
                    semaphore.acquire();
                    std::lock_guard lock(mtx);
                    if (!firstError)
                        ready.push(std::move(trg));
                    else
                        semaphore.release();
                    cv.notify_one();
                };

                try {
                    ParseChunk(data, chunks[idx].first, chunks[idx].second,
                               schema, idx, opts.rowGroupSize, emit);
                } catch (...) {
                    std::lock_guard lock(mtx);
                    if (!firstError) firstError = std::current_exception();
                    cv.notify_one();
                }
            }
        });
    }

    IO::FormatWriter writer(opts.iyxPath);
    writer.Begin(schema);

    size_t expectedChunk = 0;
    size_t expectedRg    = 0;

    while (expectedChunk < chunks.size()) {
        std::unique_lock lock(mtx);
        cv.wait(lock, [&] {
            return firstError || (!ready.empty()
                && ready.top().chunkIdx == expectedChunk
                && ready.top().rgIdx    == expectedRg);
        });

        if (firstError) {
            for (auto& w : workers) w.request_stop();
            lock.unlock();
            workers.clear();
            std::rethrow_exception(firstError);
        }

        TaggedRowGroup trg = std::move(const_cast<TaggedRowGroup&>(ready.top()));
        ready.pop();
        lock.unlock();

        if (trg.rg.GetRowCount() > 0)
            writer.AppendBlob(trg.rg);

        semaphore.release();

        if (trg.isLastInChunk) {
            ++expectedChunk;
            expectedRg = 0;
        } else {
            ++expectedRg;
        }
    }

    workers.clear();
    if (firstError) std::rethrow_exception(firstError);

    writer.End();
}

}  // namespace Columnar
