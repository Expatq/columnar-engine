#include "converter.h"

#include <io/binary/mmap_file/mmap_file.h>
#include <io/format/format_writer.h>
#include <parser/csv/csv_parser.h>
#include <parser/format/schema_parser.h>
#include <parser/format/value_parser.h>

#include <core/column.h>
#include <core/row_group.h>
#include <core/schema.h>
#include <core/types.h>
#include <util/int128.h>

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace Columnar {

namespace {

constexpr size_t kChunksPerThread = 8;
constexpr mode_t kSegmentFileMode = 0644;

std::vector<std::pair<size_t, size_t>> SplitFile(
    const uint8_t* data, size_t fileSize, size_t numChunks) {
    std::vector<std::pair<size_t, size_t>> result;
    result.reserve(numChunks);

    const size_t targetSize = fileSize / numChunks;
    size_t chunkStart = 0;
    size_t pos = 0;
    bool inQuotes = false;

    for (size_t i = 0; i < numChunks; ++i) {
        if (i == numChunks - 1 || chunkStart >= fileSize) {
            if (chunkStart < fileSize)
                result.push_back({chunkStart, fileSize - chunkStart});
            break;
        }

        const size_t target = chunkStart + targetSize;

        while (pos < target && pos < fileSize) {
            if (data[pos] == '"') {
                if (inQuotes && pos + 1 < fileSize && data[pos + 1] == '"')
                    pos += 2;
                else {
                    inQuotes = !inQuotes;
                    ++pos;
                }
            } else {
                ++pos;
            }
        }

        while (pos < fileSize) {
            if (data[pos] == '"') {
                if (inQuotes && pos + 1 < fileSize && data[pos + 1] == '"')
                    pos += 2;
                else {
                    inQuotes = !inQuotes;
                    ++pos;
                }
            } else if (data[pos] == '\n' && !inQuotes) {
                ++pos;
                break;
            } else {
                ++pos;
            }
        }

        result.push_back({chunkStart, pos - chunkStart});
        chunkStart = pos;
    }

    return result;
}

struct TaggedRowGroup {
    size_t chunkIdx;
    size_t rgIdx;
    bool isLastInChunk;
    RowGroup rg;
};

struct SegmentEntry {
    size_t chunkIdx;
    size_t rgIdx;
    uint64_t blobOffset;
    uint64_t blobSize;
    uint32_t rowCount;
    std::vector<ColStats> stats;
};

struct WorkerSegment {
    std::string path;
    int fd = -1;
    uint64_t pos = 0;
    std::vector<SegmentEntry> entries;
};

void AppendParsedToBuffer(Types::AnyColumnData& buf, const Types::AnyPhysicalType& val) {
    std::visit(
        Types::overloaded{
            [&](std::vector<int16_t>& v) { v.push_back(std::get<int16_t>(val)); },
            [&](std::vector<int32_t>& v) { v.push_back(std::get<int32_t>(val)); },
            [&](std::vector<int64_t>& v) { v.push_back(std::get<int64_t>(val)); },
            [&](std::vector<uint8_t>& v) { v.push_back(std::get<uint8_t>(val)); },
            [&](std::vector<std::string>& v) { v.push_back(std::get<std::string>(val)); },
            [&](std::vector<Int128>& v) { v.push_back(std::get<Int128>(val)); },
        },
        buf);
}

void ParseChunk(
    const uint8_t* data, size_t offset, size_t length,
    const Schema& schema, size_t chunkIdx, size_t rowGroupSize,
    std::function<void(TaggedRowGroup)> emit) {
    const size_t colCount = schema.GetColumnCount();

    std::vector<Types::AnyColumnData> bufs;
    bufs.reserve(colCount);
    for (size_t i = 0; i < colCount; ++i) {
        bufs.push_back(Types::CreateEmptyColumnData(schema.GetColumn(i).physical));
        std::visit([&](auto& v) { v.reserve(rowGroupSize); }, bufs.back());
    }

    size_t rowsInRg = 0;
    size_t rgIdx = 0;

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
        std::string row;
        bool inQuotes = false;

        while (ptr < end) {
            const char* nl = static_cast<const char*>(std::memchr(ptr, '\n', end - ptr));
            const char* lineEnd = nl ? nl : end;

            std::string_view seg(ptr, lineEnd - ptr);
            if (!seg.empty() && seg.back() == '\r')
                seg.remove_suffix(1);
            ptr = lineEnd + (nl ? 1 : 0);

            if (!row.empty())
                row += '\n';
            row.append(seg);

            for (size_t k = 0; k < seg.size(); ++k) {
                if (seg[k] == '"') {
                    if (inQuotes && k + 1 < seg.size() && seg[k + 1] == '"') {
                        ++k;
                    } else {
                        inQuotes = !inQuotes;
                    }
                }
            }
            if (!inQuotes) {
                break;
            }
        }

        if (row.empty()) {
            continue;
        }

        auto fields = Parser::ParseCsvLine(row);
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
        if (rowsInRg >= rowGroupSize)
            flushRg(/*isLast=*/false);
    }

    flushRg(/*isLast=*/true);
}

// pwrite/pread loops that handle short writes/reads.

void WriteAll(int fd, uint64_t offset, const void* src, size_t n) {
    const auto* p = static_cast<const uint8_t*>(src);
    while (n > 0) {
        const ssize_t w = ::pwrite(fd, p, n, static_cast<off_t>(offset));
        if (w < 0) {
            if (errno == EINTR)
                continue;
            throw std::runtime_error(
                std::string{"pwrite failed: "} + std::strerror(errno));
        }
        if (w == 0)
            throw std::runtime_error("pwrite returned 0");
        n -= static_cast<size_t>(w);
        p += w;
        offset += static_cast<uint64_t>(w);
    }
}

void ReadAll(int fd, uint64_t offset, void* dst, size_t n) {
    auto* p = static_cast<uint8_t*>(dst);
    while (n > 0) {
        const ssize_t r = ::pread(fd, p, n, static_cast<off_t>(offset));
        if (r < 0) {
            if (errno == EINTR)
                continue;
            throw std::runtime_error(
                std::string{"pread failed: "} + std::strerror(errno));
        }
        if (r == 0)
            throw std::runtime_error("unexpected EOF in segment file");
        n -= static_cast<size_t>(r);
        p += r;
        offset += static_cast<uint64_t>(r);
    }
}

void CleanupSegments(std::vector<WorkerSegment>& segments) {
    for (auto& s : segments) {
        if (s.fd >= 0) {
            ::close(s.fd);
            s.fd = -1;
        }
        if (!s.path.empty())
            ::unlink(s.path.c_str());
    }
}

void MergeSegmentsIntoWriter(IO::FormatWriter& writer,
                             std::vector<WorkerSegment>& segments) {
    using Ref = std::pair<size_t, size_t>;  // (segmentIdx, entryIdx)
    std::vector<Ref> refs;
    size_t total = 0;
    for (const auto& s : segments) total += s.entries.size();
    refs.reserve(total);

    for (size_t s = 0; s < segments.size(); ++s)
        for (size_t e = 0; e < segments[s].entries.size(); ++e)
            refs.emplace_back(s, e);

    std::sort(refs.begin(), refs.end(), [&](Ref a, Ref b) {
        const auto& ea = segments[a.first].entries[a.second];
        const auto& eb = segments[b.first].entries[b.second];
        if (ea.chunkIdx != eb.chunkIdx)
            return ea.chunkIdx < eb.chunkIdx;
        return ea.rgIdx < eb.rgIdx;
    });

    std::vector<uint8_t> buf;
    for (auto [s, e] : refs) {
        const auto& seg = segments[s];
        auto& ent = segments[s].entries[e];
        if (ent.rowCount == 0)
            continue;

        buf.resize(ent.blobSize);
        ReadAll(seg.fd, ent.blobOffset, buf.data(), ent.blobSize);

        IO::FormatWriter::EncodedRowGroup enc;
        enc.blob = std::move(buf);
        enc.stats = std::move(ent.stats);
        enc.rowCount = ent.rowCount;
        writer.AppendEncoded(std::move(enc));

        buf.clear();
    }
}

}  // namespace

void Run(const ConvertOptions& opts) {
    const Schema schema = Parser::LoadSchemaFromCsv(opts.schemaPath);

    const size_t numThreads = opts.numThreads > 0
                                  ? opts.numThreads
                                  : std::thread::hardware_concurrency();

    IO::MmapFile csvFile(opts.csvPath);
    const uint8_t* data = csvFile.Ptr(0);
    const size_t fileSize = csvFile.GetFileSize();

    const size_t numChunks = numThreads * kChunksPerThread;
    auto chunks = SplitFile(data, fileSize, numChunks);

    std::vector<WorkerSegment> segments(numThreads);
    for (size_t t = 0; t < numThreads; ++t) {
        segments[t].path = opts.iyxPath + ".seg." + std::to_string(t);
        segments[t].fd = ::open(segments[t].path.c_str(),
                                O_RDWR | O_CREAT | O_TRUNC,
                                kSegmentFileMode);
        if (segments[t].fd < 0) {
            CleanupSegments(segments);
            throw std::runtime_error(
                "cannot create segment file " + segments[t].path +
                ": " + std::strerror(errno));
        }
    }

    std::atomic<size_t> nextChunk{0};
    std::atomic<bool> abort{false};
    std::mutex errMtx;
    std::exception_ptr firstError;

    std::vector<std::thread> workers;
    workers.reserve(numThreads);

    for (size_t t = 0; t < numThreads; ++t) {
        workers.emplace_back([&, t]() {
            WorkerSegment& seg = segments[t];

            while (!abort.load(std::memory_order_relaxed)) {
                const size_t idx = nextChunk.fetch_add(1, std::memory_order_relaxed);
                if (idx >= chunks.size())
                    break;

                auto emit = [&](TaggedRowGroup trg) {
                    if (trg.rg.GetRowCount() == 0)
                        return;
                    auto enc = IO::FormatWriter::EncodeRowGroup(trg.rg);
                    const uint64_t blobOffset = seg.pos;
                    const uint64_t blobSize = enc.blob.size();
                    WriteAll(seg.fd, blobOffset, enc.blob.data(), blobSize);
                    seg.pos += blobSize;
                    seg.entries.push_back(SegmentEntry{
                        trg.chunkIdx, trg.rgIdx,
                        blobOffset, blobSize, enc.rowCount,
                        std::move(enc.stats)});
                };

                try {
                    ParseChunk(data, chunks[idx].first, chunks[idx].second,
                               schema, idx, opts.rowGroupSize, emit);
                } catch (...) {
                    {
                        std::lock_guard lock(errMtx);
                        if (!firstError)
                            firstError = std::current_exception();
                    }
                    abort.store(true, std::memory_order_relaxed);
                    return;
                }
            }
        });
    }

    for (auto& w : workers) w.join();

    if (firstError) {
        CleanupSegments(segments);
        std::rethrow_exception(firstError);
    }

    try {
        IO::FormatWriter writer(opts.iyxPath);
        writer.Begin(schema);
        MergeSegmentsIntoWriter(writer, segments);
        writer.End();
    } catch (...) {
        CleanupSegments(segments);
        throw;
    }

    CleanupSegments(segments);
}

}  // namespace Columnar
