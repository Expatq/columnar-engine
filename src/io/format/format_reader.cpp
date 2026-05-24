#include <io/format/bitpack.h>
#include <io/format/format_defs.h>
#include <io/format/format_reader.h>

#include <core/types.h>

#include <util/assert.h>
#include <util/int128.h>

#include <cstdint>
#include <cstring>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace Columnar::IO {

namespace {

class BufCursor {
public:
    explicit BufCursor(const uint8_t* data)
        : data_(data) {
    }

    template <typename T>
    T Read() {
        T v;
        std::memcpy(&v, data_ + pos_, sizeof(T));
        pos_ += sizeof(T);
        return v;
    }

private:
    const uint8_t* data_;
    size_t pos_ = 0;
};

}  // namespace

FormatReader::FormatReader(const std::string& filename)
    : file_(filename) {
    ValidateMagic();
    ReadHeader();
    ReadSchema();
    ReadFooter();
}

std::string FormatReader::ReadString() {
    const uint32_t len = ReadField<uint32_t>();
    if (len == 0)
        return {};
    std::string s(len, '\0');
    ReadBytes(s.data(), len);
    return s;
}

size_t FormatReader::EstimateColSize(Types::PhysicalType type, size_t rowCount) {
    switch (type) {
        case Types::PhysicalType::INT16:
            return rowCount * 2 + 10;
        case Types::PhysicalType::INT32:
            return rowCount * 4 + 10;
        case Types::PhysicalType::INT64:
            return rowCount * 8 + 1;
        case Types::PhysicalType::INT128:
            return rowCount * 16 + 1;
        case Types::PhysicalType::BOOL:
            return rowCount + 1;
        default:
            return rowCount * kStringAvgEstimatedBytes;
    }
}

void FormatReader::ValidateMagic() {
    const size_t fileSize = file_.GetFileSize();
    if (fileSize <= kMagicSize + kHeaderSize) {
        throw std::runtime_error("file too small to be valid .iyx");
    }

    uint8_t magic[kMagicSize];
    pos_ = fileSize - kMagicSize;
    ReadBytes(magic, kMagicSize);

    for (size_t i = 0; i < kMagicSize; ++i) {
        if (magic[i] != kMagicBytes[i]) {
            throw std::runtime_error("invalid .iyx magic");
        }
    }

    pos_ = 0;
}

void FormatReader::ReadHeader() {
    pos_ = 0;

    columnCount_ = ReadField<uint32_t>();
    rgCount_ = ReadField<uint32_t>();
    totalRowCount_ = ReadField<uint64_t>();

    const uint64_t schemaOffset = ReadField<uint64_t>();
    footerOffset_ = ReadField<uint64_t>();

    if (schemaOffset != kHeaderSize)
        throw std::runtime_error("unsupported schema offset");
    if (columnCount_ == 0)
        throw std::runtime_error("file has empty schema");

    rowGroupMetas_.reserve(rgCount_);
    pos_ = kHeaderSize;
}

void FormatReader::ReadSchema() {
    for (uint32_t i = 0; i < columnCount_; ++i) {
        const uint8_t type = ReadField<uint8_t>();
        std::string name = ReadString();
        schema_.AddColumn(name, static_cast<Types::LogicalType>(type));
    }
}

void FormatReader::ReadFooter() {
    const size_t metaSize = sizeof(uint32_t) + rgCount_ * sizeof(uint64_t) + rgCount_ * sizeof(uint32_t) + sizeof(uint8_t);

    std::vector<uint8_t> metaBuf(metaSize);
    file_.Read(footerOffset_, metaBuf.data(), metaSize);

    BufCursor cur(metaBuf.data());
    cur.Read<uint32_t>();

    rowGroupMetas_.resize(rgCount_);
    for (auto& m : rowGroupMetas_) m.offset = cur.Read<uint64_t>();
    for (auto& m : rowGroupMetas_) m.rowCount = cur.Read<uint32_t>();

    const uint8_t hasStats = cur.Read<uint8_t>();
    if (!hasStats) {
        return;
    }

    allStats_.resize(rgCount_ * columnCount_);
    file_.Read(footerOffset_ + metaSize,
               allStats_.data(),
               allStats_.size() * sizeof(ColStats));
}

std::optional<RowGroup> FormatReader::ReadRowGroup() {
    if (curRowGroupIdx_ >= rowGroupMetas_.size())
        return std::nullopt;
    return ReadAllColumns(rowGroupMetas_[curRowGroupIdx_++]);
}

std::optional<RowGroup> FormatReader::ReadRowGroup(std::span<const std::string> colNames) {
    if (curRowGroupIdx_ >= rowGroupMetas_.size())
        return std::nullopt;
    if (colNames.empty())
        throw std::invalid_argument("selected column list cannot be empty");
    return ReadSelectedColumns(rowGroupMetas_[curRowGroupIdx_++],
                               ResolveColumnNames(colNames));
}

bool FormatReader::HasMore() const {
    return curRowGroupIdx_ < rowGroupMetas_.size();
}

const Schema& FormatReader::GetSchema() const {
    return schema_;
}

size_t FormatReader::GetRowGroupCount() const {
    return rowGroupMetas_.size();
}

const RowGroupMeta& FormatReader::GetRowGroupMeta(size_t index) const {
    if (index >= rowGroupMetas_.size())
        throw std::out_of_range("row group index out of range");
    return rowGroupMetas_[index];
}

uint64_t FormatReader::GetTotalRowCount() const {
    return totalRowCount_;
}

uint32_t FormatReader::GetRowGroupRows(size_t index) const {
    if (index >= rowGroupMetas_.size()) {
        throw std::out_of_range("row group index out of range");
    }
    return rowGroupMetas_[index].rowCount;
}

std::vector<size_t> FormatReader::ResolveColumnNames(
    std::span<const std::string> colNames) const {
    std::vector<size_t> indices;
    indices.reserve(colNames.size());
    for (const auto& name : colNames) {
        auto idx = schema_.FindColumn(name);
        if (!idx)
            throw std::invalid_argument("unknown column '" + name + "'");
        indices.push_back(*idx);
    }
    return indices;
}

RowGroup FormatReader::ReadAllColumns(const RowGroupMeta& meta) {
    std::vector<size_t> colIndices(schema_.GetColumnCount());
    std::iota(colIndices.begin(), colIndices.end(), 0);
    return ReadSelectedColumns(meta, colIndices);
}

RowGroup FormatReader::ReadSelectedColumns(const RowGroupMeta& meta,
                                           const std::vector<size_t>& colIndices) {
    if (colIndices.empty())
        throw std::invalid_argument("selected column list cannot be empty");

    pos_ = meta.offset;

    const int64_t rowCount = ReadField<int64_t>();
    if (rowCount < 0)
        throw std::runtime_error("negative row count in row group");

    const size_t columnCount = schema_.GetColumnCount();
    std::vector<int64_t> colOffsets(columnCount);
    for (size_t i = 0; i < columnCount; ++i)
        colOffsets[i] = ReadField<int64_t>();

    Schema resultSchema;
    std::vector<Column> resultColumns;
    resultColumns.reserve(colIndices.size());

    for (size_t i = 0; i < colIndices.size(); ++i) {
        const size_t colIdx = colIndices[i];
        if (colIdx >= columnCount)
            throw std::out_of_range("selected column index out of range");
        if (colOffsets[colIdx] < 0)
            throw std::runtime_error("negative column offset");

        if (i + 1 < colIndices.size()) {
            const size_t nextIdx = colIndices[i + 1];
            const size_t nextOffset = static_cast<size_t>(
                static_cast<int64_t>(meta.offset) + colOffsets[nextIdx]);
            const size_t nextSize = EstimateColSize(
                schema_.GetColumn(nextIdx).physical,
                static_cast<size_t>(rowCount));
            file_.Prefetch(nextOffset, nextSize);
        }

        pos_ = static_cast<size_t>(static_cast<int64_t>(meta.offset) + colOffsets[colIdx]);

        const auto& schemaCol = schema_.GetColumn(colIdx);
        resultSchema.AddColumn(schemaCol);
        resultColumns.push_back(
            ReadColumn(schemaCol.physical, static_cast<size_t>(rowCount)));
    }

    return RowGroup(std::move(resultSchema), std::move(resultColumns));
}

Column FormatReader::ReadColumn(Types::PhysicalType physical, size_t rowCount) {
    switch (physical) {
        case Types::PhysicalType::INT16: {
            const uint8_t enc = ReadField<uint8_t>();
            std::vector<int16_t> values(rowCount);
            if (enc == kEncBitpack) {
                const int64_t minVal = ReadField<int64_t>();
                const uint8_t bitWidth = ReadField<uint8_t>();
                const size_t packedN = PackedBytes(rowCount, bitWidth);
                std::vector<uint8_t> packed(packedN);
                if (!packed.empty())
                    ReadBytes(packed.data(), packedN);
                if (!values.empty())
                    BitpackDecodeI16(packed.data(), rowCount, bitWidth, minVal, values.data());
            } else {
                if (!values.empty())
                    ReadBytes(values.data(), rowCount * sizeof(int16_t));
            }
            return Column(std::move(values), physical);
        }
        case Types::PhysicalType::INT32: {
            const uint8_t enc = ReadField<uint8_t>();
            std::vector<int32_t> values(rowCount);
            if (enc == kEncBitpack) {
                const int64_t minVal = ReadField<int64_t>();
                const uint8_t bitWidth = ReadField<uint8_t>();
                const size_t packedN = PackedBytes(rowCount, bitWidth);
                std::vector<uint8_t> packed(packedN);
                if (!packed.empty())
                    ReadBytes(packed.data(), packedN);
                if (!values.empty())
                    BitpackDecodeI32(packed.data(), rowCount, bitWidth, minVal, values.data());
            } else {
                if (!values.empty())
                    ReadBytes(values.data(), rowCount * sizeof(int32_t));
            }
            return Column(std::move(values), physical);
        }
        case Types::PhysicalType::INT64: {
            ReadField<uint8_t>();
            std::vector<int64_t> values(rowCount);
            if (!values.empty())
                ReadBytes(values.data(), rowCount * sizeof(int64_t));
            return Column(std::move(values), physical);
        }
        case Types::PhysicalType::INT128: {
            ReadField<uint8_t>();
            std::vector<Int128> values(rowCount);
            if (!values.empty())
                ReadBytes(values.data(), rowCount * sizeof(Int128));
            return Column(std::move(values), physical);
        }
        case Types::PhysicalType::BOOL: {
            ReadField<uint8_t>();
            std::vector<uint8_t> values(rowCount);
            if (!values.empty())
                ReadBytes(values.data(), rowCount);
            return Column(std::move(values), physical);
        }
        case Types::PhysicalType::STRING: {
            const uint64_t totalChars = ReadField<uint64_t>();

            std::vector<uint32_t> offsets(rowCount + 1);
            ReadBytes(offsets.data(), offsets.size() * sizeof(uint32_t));

            std::string chars(totalChars, '\0');
            if (totalChars > 0)
                ReadBytes(chars.data(), totalChars);

            std::vector<std::string> values(rowCount);
            for (size_t i = 0; i < rowCount; ++i)
                values[i].assign(chars.data() + offsets[i], offsets[i + 1] - offsets[i]);

            return Column(std::move(values), physical);
        }
        default:
            COLUMNAR_ASSERT(false, "unknown PhysicalType");
    }
}

}  // namespace Columnar::IO
