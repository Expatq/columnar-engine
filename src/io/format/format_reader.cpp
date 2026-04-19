#include "format_reader.h"

#include <io/binary/binary_io.h>

#include <core/column.h>
#include <core/row_group.h>
#include <core/types.h>

#include <util/assert.h>

#include <cstdint>
#include <stdexcept>

namespace Columnar::IO {

FormatReader::FormatReader(const std::string& filename)
    : reader_(filename) {}

void FormatReader::Open() {
    if (opened_) {
        return;
    }
    ValidateMagic();
    ReadHeader();
    ReadSchema();
    ReadFooter();
    opened_ = true;
}

void FormatReader::ValidateMagic() {
    size_t fileSize = reader_.GetFileSize();
    if (fileSize <= kMagicSize + kHeaderSize) {
        throw std::runtime_error("File too small to be valid .iyx");
    }

    reader_.Seek(fileSize - kMagicSize);
    uint8_t magic[kMagicSize];
    reader_.Read(magic, kMagicSize);
    for (size_t i = 0; i < kMagicSize; ++i) {
        if (magic[i] != kMagicBytes[i]) {
            throw std::runtime_error("Invalid .iyx magic");
        }
    }
    reader_.Seek(0);
}

void FormatReader::ReadHeader() {
    reader_.Read(&columnCount_, sizeof(columnCount_));
    uint32_t rgCount;
    reader_.Read(&rgCount, sizeof(rgCount));
    reader_.Read(&totalRowCount_, sizeof(totalRowCount_));
    uint64_t schemaOffset;
    reader_.Read(&schemaOffset, sizeof(schemaOffset));
    reader_.Read(&footerOffset_, sizeof(footerOffset_));
    reader_.Seek(kHeaderSize);
    rgMetas_.reserve(rgCount);
}

void FormatReader::ReadSchema() {
    for (uint32_t i = 0; i < columnCount_; ++i) {
        uint8_t type;
        reader_.Read(&type, sizeof(type));
        std::string name = reader_.ReadString();
        schema_.AddColumn(name, static_cast<Types::LogicalType>(type));
    }
}

void FormatReader::ReadFooter() {
    reader_.Seek(footerOffset_);

    uint32_t rgCount;
    reader_.Read(&rgCount, sizeof(rgCount));

    std::vector<uint64_t> offsets(rgCount);
    for (uint32_t i = 0; i < rgCount; ++i) {
        reader_.Read(&offsets[i], sizeof(offsets[i]));
    }

    std::vector<uint32_t> rowCounts(rgCount);
    for (uint32_t i = 0; i < rgCount; ++i) {
        reader_.Read(&rowCounts[i], sizeof(rowCounts[i]));
    }

    for (uint32_t i = 0; i < rgCount; ++i) {
        rgMetas_.push_back({offsets[i], rowCounts[i]});
    }
}

std::optional<RowGroup> FormatReader::ReadBatch() {
    if (!opened_) {
        Open();
    }
    if (currentRgIdx_ >= rgMetas_.size()) {
        return std::nullopt;
    }
    return ReadRgInternal(rgMetas_[currentRgIdx_++], {});
}

bool FormatReader::HasMore() const {
    return currentRgIdx_ < rgMetas_.size();
}

RowGroup FormatReader::ReadRowGroup(size_t index) {
    if (!opened_) {
        throw std::logic_error("Open() not called");
    }
    if (index >= rgMetas_.size()) {
        throw std::out_of_range("row group index out of range");
    }
    return ReadRgInternal(rgMetas_[index], {});
}

const Schema& FormatReader::GetSchema() const {
    return schema_;
}

size_t FormatReader::GetRowGroupCount() const {
    return rgMetas_.size();
}

uint64_t FormatReader::GetTotalRowCount() const {
    return totalRowCount_;
}

const RowGroupMeta& FormatReader::GetRowGroupMeta(size_t index) const {
    if (index >= rgMetas_.size()) {
        throw std::out_of_range("Index out of range");
    }
    return rgMetas_[index];
}

int64_t FormatReader::GetRowGroupRows(size_t index) const {
    COLUMNAR_ASSERT(index < rgMetas_.size(), "row group index out of range");
    return static_cast<int64_t>(rgMetas_[index].rowCount);
}

RowGroup FormatReader::ReadRgInternal(const RowGroupMeta& meta,
                                      const std::vector<size_t>& col_indices) {
    reader_.Seek(meta.offset);

    int64_t rowCount = 0;
    reader_.Read(&rowCount, sizeof(rowCount));

    const size_t nCols = schema_.GetColumnCount();

    std::vector<int64_t> colOffsets(nCols);
    for (size_t i = 0; i < nCols; ++i) {
        reader_.Read(&colOffsets[i], sizeof(colOffsets[i]));
    }

    const bool readAll = col_indices.empty();
    std::vector<bool> needed(nCols, readAll);
    if (!readAll) {
        for (size_t ci : col_indices) {
            COLUMNAR_ASSERT(ci < nCols, "column index out of range");
            needed[ci] = true;
        }
    }

    std::vector<Column> cols;
    cols.reserve(readAll ? nCols : col_indices.size());

    for (size_t i = 0; i < nCols; ++i) {
        if (!needed[i]) {
            continue;
        }
        reader_.Seek(static_cast<size_t>(static_cast<int64_t>(meta.offset) +
                                         colOffsets[i]));
        cols.push_back(ReadColumn(schema_.GetColumn(i).physical,
                                  static_cast<size_t>(rowCount)));
    }

    if (readAll) {
        return RowGroup(schema_, std::move(cols));
    }

    Schema partial;
    for (size_t i = 0; i < nCols; ++i) {
        if (needed[i]) {
            partial.AddColumn(schema_.GetColumn(i));
        }
    }
    return RowGroup(std::move(partial), std::move(cols));
}

Column FormatReader::ReadColumn(Types::PhysicalType physical, size_t rowCount) {
    Types::AnyColumnData data;

    switch (physical) {
        case Types::PhysicalType::INT16: {
            std::vector<int16_t> v;
            v.reserve(rowCount);
            for (size_t i = 0; i < rowCount; ++i) {
                int16_t x;
                reader_.Read(&x, sizeof(x));
                v.push_back(x);
            }
            data = std::move(v);
            break;
        }
        case Types::PhysicalType::INT32: {
            std::vector<int32_t> v;
            v.reserve(rowCount);
            for (size_t i = 0; i < rowCount; ++i) {
                int32_t x;
                reader_.Read(&x, sizeof(x));
                v.push_back(x);
            }
            data = std::move(v);
            break;
        }
        case Types::PhysicalType::INT64: {
            std::vector<int64_t> v;
            v.reserve(rowCount);
            for (size_t i = 0; i < rowCount; ++i) {
                int64_t x;
                reader_.Read(&x, sizeof(x));
                v.push_back(x);
            }
            data = std::move(v);
            break;
        }
        case Types::PhysicalType::BOOL: {
            std::vector<bool> v;
            v.reserve(rowCount);
            for (size_t i = 0; i < rowCount; ++i) {
                uint8_t b;
                reader_.Read(&b, sizeof(b));
                v.push_back(b != 0);
            }
            data = std::move(v);
            break;
        }
        case Types::PhysicalType::STRING: {
            std::vector<std::string> v;
            v.reserve(rowCount);
            for (size_t i = 0; i < rowCount; ++i) {
                v.push_back(reader_.ReadString());
            }
            data = std::move(v);
            break;
        }
        default:
            COLUMNAR_ASSERT(false, "ReadColumn: unknown PhysicalType");
            throw std::logic_error("ReadColumn: unknown PhysicalType");
    }

    return Column(std::move(data), physical);
}

}  // namespace Columnar::IO
