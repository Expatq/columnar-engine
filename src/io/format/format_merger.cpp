#include "format_merger.h"

#include <io/binary/file_ops.h>
#include <io/format/format_defs.h>
#include <io/format/format_reader.h>

#include <core/col_stats.h>
#include <core/schema.h>

#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace Columnar::IO {

namespace {

class UniqueFd {
public:
    UniqueFd(const std::filesystem::path& path, int flags, mode_t mode = 0644) {
        fd_ = ::open(path.c_str(), flags, mode);
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

template <typename T>
void WriteValue(int fd, uint64_t* pos, const T& value) {
    WriteAll(fd, &value, sizeof(T), *pos);
    *pos += sizeof(T);
}

void WriteBytes(int fd, uint64_t* pos, const void* data, size_t size) {
    if (size == 0) {
        return;
    }
    WriteAll(fd, data, size, *pos);
    *pos += size;
}

void WriteString(int fd, uint64_t* pos, const std::string& value) {
    const uint32_t size = static_cast<uint32_t>(value.size());
    WriteValue(fd, pos, size);
    WriteBytes(fd, pos, value.data(), value.size());
}

void WriteHeaderPlaceholder(int fd, uint64_t* pos, const Schema& schema) {
    const uint32_t columnCount = static_cast<uint32_t>(schema.GetColumnCount());
    const uint32_t rowGroupCount = 0;
    const uint64_t totalRows = 0;
    const uint64_t schemaOffset = kHeaderSize;
    const uint64_t footerOffset = 0;
    char reserved[32] = {};

    WriteValue(fd, pos, columnCount);
    WriteValue(fd, pos, rowGroupCount);
    WriteValue(fd, pos, totalRows);
    WriteValue(fd, pos, schemaOffset);
    WriteValue(fd, pos, footerOffset);
    WriteBytes(fd, pos, reserved, sizeof(reserved));
}

void WriteSchema(int fd, uint64_t* pos, const Schema& schema) {
    for (const auto& column : schema) {
        const uint8_t type = static_cast<uint8_t>(column.logical);
        WriteValue(fd, pos, type);
        WriteString(fd, pos, column.name);
    }
}

void PatchHeader(int fd,
                 uint32_t rowGroupCount,
                 uint64_t totalRows,
                 uint64_t footerOffset) {
    WriteAll(fd, &rowGroupCount, sizeof(rowGroupCount), 4);
    WriteAll(fd, &totalRows, sizeof(totalRows), 8);
    WriteAll(fd, &footerOffset, sizeof(footerOffset), 24);
}

}  // namespace

MergeStats MergeIyxFiles(const std::vector<MergeInput>& inputs,
                         const std::filesystem::path& output) {
    if (inputs.empty()) {
        throw std::invalid_argument("MergeIyxFiles: inputs are empty");
    }

    FormatReader firstReader(inputs[0].path.string());
    const Schema& schema = firstReader.GetSchema();

    const std::filesystem::path tmpPath = output.string() + ".tmp";
    UniqueFd out(tmpPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    uint64_t pos = 0;
    WriteHeaderPlaceholder(out.Get(), &pos, schema);
    WriteSchema(out.Get(), &pos, schema);

    std::vector<uint64_t> offsets;
    std::vector<uint32_t> rows;
    std::vector<ColStats> stats;
    bool hasStats = true;

    MergeStats result;

    for (const auto& input : inputs) {
        FormatReader reader(input.path.string());
        if (!(reader.GetSchema() == schema)) {
            throw std::runtime_error("MergeIyxFiles: schema mismatch in " +
                                     input.path.string());
        }

        UniqueFd in(input.path, O_RDONLY);
        const auto ranges = reader.GetRawRowGroupRanges();
        const auto inputStats = reader.GetAllStats();

        if (inputStats.empty()) {
            hasStats = false;
        }

        for (const auto& range : ranges) {
            offsets.push_back(pos);
            rows.push_back(range.rows);

            CopyFileRange(in.Get(), range.offset, out.Get(), pos, range.size);
            pos += range.size;

            result.bytesCopied += range.size;
            result.rows += range.rows;
            ++result.rowGroups;
        }

        if (hasStats && !inputStats.empty()) {
            stats.insert(stats.end(), inputStats.begin(), inputStats.end());
        }
    }

    const uint64_t footerOffset = pos;
    const uint32_t rowGroupCount = static_cast<uint32_t>(offsets.size());

    WriteValue(out.Get(), &pos, rowGroupCount);
    for (uint64_t offset : offsets) {
        WriteValue(out.Get(), &pos, offset);
    }
    for (uint32_t rowCount : rows) {
        WriteValue(out.Get(), &pos, rowCount);
    }

    const uint8_t hasStatsByte = hasStats && !stats.empty() ? 1 : 0;
    WriteValue(out.Get(), &pos, hasStatsByte);
    if (hasStatsByte) {
        WriteBytes(out.Get(), &pos, stats.data(), stats.size() * sizeof(ColStats));
    }

    WriteBytes(out.Get(), &pos, kMagicBytes, kMagicSize);
    PatchHeader(out.Get(), rowGroupCount, result.rows, footerOffset);

    if (::fsync(out.Get()) < 0) {
        throw std::runtime_error("MergeIyxFiles: fsync failed for " + tmpPath.string());
    }

    if (::rename(tmpPath.c_str(), output.c_str()) < 0) {
        throw std::runtime_error("MergeIyxFiles: rename failed for " + output.string());
    }

    return result;
}

}  // namespace Columnar::IO
