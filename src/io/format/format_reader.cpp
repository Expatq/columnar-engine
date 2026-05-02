#include <io/format/format_reader.h>

#include <util/assert.h>

#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace Columnar::IO {

FormatReader::FormatReader(const std::string& filename)
    : reader_(filename) {
    ValidateMagic();
    ReadHeader();
    ReadSchema();
    ReadFooter();
}

void FormatReader::ValidateMagic() {
    const size_t fileSize = reader_.GetFileSize();
    if (fileSize <= kMagicSize + kHeaderSize) {
        throw std::runtime_error("file too small to be valid .iyx");
    }

    uint8_t magic[kMagicSize];
    reader_.Seek(fileSize - kMagicSize);
    reader_.Read(magic, kMagicSize);

    for (size_t i = 0; i < kMagicSize; ++i) {
        if (magic[i] != kMagicBytes[i]) {
            throw std::runtime_error("invalid .iyx magic");
        }
    }

    reader_.Seek(0);
}

void FormatReader::ReadHeader() {
    reader_.Seek(0);

    uint32_t rowGroupCount = 0;
    uint64_t schemaOffset = 0;

    reader_.Read(&columnCount_, sizeof(columnCount_));
    reader_.Read(&rowGroupCount, sizeof(rowGroupCount));
    reader_.Read(&totalRowCount_, sizeof(totalRowCount_));
    reader_.Read(&schemaOffset, sizeof(schemaOffset));
    reader_.Read(&footerOffset_, sizeof(footerOffset_));

    if (schemaOffset != kHeaderSize) {
        throw std::runtime_error("unsupported schema offset");
    }
    if (columnCount_ == 0) {
        throw std::runtime_error("file has empty schema");
    }

    rowGroupMetas_.reserve(rowGroupCount);
    reader_.Seek(kHeaderSize);
}

void FormatReader::ReadSchema() {
    for (uint32_t i = 0; i < columnCount_; ++i) {
        uint8_t type = 0;
        reader_.Read(&type, sizeof(type));
        std::string name = reader_.ReadString();
        schema_.AddColumn(name, static_cast<Types::LogicalType>(type));
    }
}

void FormatReader::ReadFooter() {
    reader_.Seek(footerOffset_);

    uint32_t rowGroupCount = 0;
    reader_.Read(&rowGroupCount, sizeof(rowGroupCount));

    std::vector<uint64_t> offsets(rowGroupCount);
    for (uint32_t i = 0; i < rowGroupCount; ++i) {
        reader_.Read(&offsets[i], sizeof(offsets[i]));
    }

    std::vector<uint32_t> rows(rowGroupCount);
    for (uint32_t i = 0; i < rowGroupCount; ++i) {
        reader_.Read(&rows[i], sizeof(rows[i]));
    }

    rowGroupMetas_.clear();
    rowGroupMetas_.reserve(rowGroupCount);
    for (uint32_t i = 0; i < rowGroupCount; ++i) {
        rowGroupMetas_.push_back(RowGroupMeta{offsets[i], rows[i]});
    }
}

std::optional<RowGroup> FormatReader::ReadRowGroup() {
    if (curRowGroupIdx_ >= rowGroupMetas_.size()) {
        return std::nullopt;
    }
    return ReadAllColumns(rowGroupMetas_[curRowGroupIdx_++]);
}

std::optional<RowGroup> FormatReader::ReadRowGroup(
    const std::vector<std::string>& colNames) {
    if (curRowGroupIdx_ >= rowGroupMetas_.size()) {
        return std::nullopt;
    }
    if (colNames.empty()) {
        throw std::invalid_argument("selected column list cannot be empty");
    }

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
    if (index >= rowGroupMetas_.size()) {
        throw std::out_of_range("row group index out of range");
    }
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
    const std::vector<std::string>& colNames) const {
    std::vector<size_t> indices;
    indices.reserve(colNames.size());

    for (const auto& name : colNames) {
        auto idx = schema_.FindColumn(name);
        if (!idx) {
            throw std::invalid_argument("unknown column '" + name + "'");
        }
        indices.push_back(*idx);
    }

    return indices;
}

RowGroup FormatReader::ReadAllColumns(const RowGroupMeta& meta) {
    std::vector<size_t> colIndices(schema_.GetColumnCount());
    std::iota(colIndices.begin(), colIndices.end(), 0);
    return ReadSelectedColumns(meta, colIndices);
}

RowGroup FormatReader::ReadSelectedColumns(
    const RowGroupMeta& meta, const std::vector<size_t>& colIndices) {
    if (colIndices.empty()) {
        throw std::invalid_argument("selected column list cannot be empty");
    }

    reader_.Seek(meta.offset);

    int64_t rowCount = 0;
    reader_.Read(&rowCount, sizeof(rowCount));
    if (rowCount < 0) {
        throw std::runtime_error("negative row count in row group");
    }

    const size_t columnCount = schema_.GetColumnCount();
    std::vector<int64_t> colOffsets(columnCount);
    for (size_t i = 0; i < columnCount; ++i) {
        reader_.Read(&colOffsets[i], sizeof(colOffsets[i]));
    }

    Schema resultSchema;
    std::vector<Column> resultColumns;
    resultColumns.reserve(colIndices.size());

    for (size_t columnIndex : colIndices) {
        if (columnIndex >= columnCount) {
            throw std::out_of_range("selected column index out of range");
        }

        const int64_t relativeOffset = colOffsets[columnIndex];
        if (relativeOffset < 0) {
            throw std::runtime_error("negative column offset");
        }

        const auto& schemaColumn = schema_.GetColumn(columnIndex);
        resultSchema.AddColumn(schemaColumn);

        reader_.Seek(static_cast<size_t>(
            static_cast<int64_t>(meta.offset) + relativeOffset));
        resultColumns.push_back(ReadColumn(schemaColumn.physical,
                                           static_cast<size_t>(rowCount)));
    }

    return RowGroup(std::move(resultSchema), std::move(resultColumns));
}

Column FormatReader::ReadColumn(Types::PhysicalType physical, size_t rowCount) {
    switch (physical) {
        case Types::PhysicalType::INT16: {
            std::vector<int16_t> values(rowCount);
            if (!values.empty()) {
                reader_.Read(values.data(), values.size() * sizeof(values[0]));
            }
            return Column(std::move(values), physical);
        }
        case Types::PhysicalType::INT32: {
            std::vector<int32_t> values(rowCount);
            if (!values.empty()) {
                reader_.Read(values.data(), values.size() * sizeof(values[0]));
            }
            return Column(std::move(values), physical);
        }
        case Types::PhysicalType::INT64: {
            std::vector<int64_t> values(rowCount);
            if (!values.empty()) {
                reader_.Read(values.data(), values.size() * sizeof(values[0]));
            }
            return Column(std::move(values), physical);
        }
        case Types::PhysicalType::BOOL: {
            std::vector<uint8_t> values(rowCount);
            if (!values.empty()) {
                reader_.Read(values.data(), values.size() * sizeof(values[0]));
            }
            return Column(std::move(values), physical);
        }
        case Types::PhysicalType::STRING: {
            std::vector<std::string> values;
            values.reserve(rowCount);
            for (size_t i = 0; i < rowCount; ++i) {
                values.push_back(reader_.ReadString());
            }
            return Column(std::move(values), physical);
        }
        default:
            COLUMNAR_ASSERT(false, "unknown PhysicalType");
    }
}

}  // namespace Columnar::IO
