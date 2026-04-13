#include <core/types.h>

#include <io/binary_io.h>
#include <io/format_reader.h>

#include <cstdint>
#include <stdexcept>
#include "core/row_group.h"

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
        throw std::runtime_error("File too small to be valid .iyx format");
    }

    reader_.Seek(fileSize - kMagicSize);
    uint8_t magic[kMagicSize];
    reader_.Read(magic, kMagicSize);

    for (size_t i = 0; i < kMagicSize; ++i) {
        if (magic[i] != kMagicBytes[i]) {
            throw std::runtime_error("Invalid magic bytes");
        }
    }

    reader_.Seek(0);
}

void FormatReader::ReadHeader() {
    reader_.Read(&columnCount_, sizeof(columnCount_));

    uint32_t rowGroupCount;
    reader_.Read(&rowGroupCount, sizeof(rowGroupCount));
    reader_.Read(&totalRowCount_, sizeof(totalRowCount_));

    uint64_t schemaOffset;
    reader_.Read(&schemaOffset, sizeof(schemaOffset));
    reader_.Read(&footerOffset_, sizeof(footerOffset_));

    reader_.Seek(kHeaderSize);
    rowGroupMetas_.reserve(rowGroupCount);
}

void FormatReader::ReadSchema() {
    for (uint32_t i = 0; i < columnCount_; ++i) {
        uint8_t type;
        reader_.Read(&type, sizeof(type));
        std::string name = reader_.ReadString();
        schema_.AddColumn(name, static_cast<Types::DataType>(type));
    }
}

void FormatReader::ReadFooter() {
    reader_.Seek(footerOffset_);

    size_t footerEnd = reader_.GetFileSize() - kMagicSize;
    size_t count = (footerEnd - footerOffset_) / RowGroupMeta::kSerializedSize;

    for (size_t i = 0; i < count; ++i) {
        RowGroupMeta meta;
        reader_.Read(&meta.offset, sizeof(meta.offset));
        reader_.Read(&meta.size, sizeof(meta.size));
        reader_.Read(&meta.rowCount, sizeof(meta.rowCount));
        rowGroupMetas_.push_back(meta);
    }
}

std::optional<Batch> FormatReader::ReadBatch() {
    if (!opened_)
        Open();
    if (currentRowGroupIndex_ >= rowGroupMetas_.size())
        return std::nullopt;

    RowGroup rg = ReadRowGroup(currentRowGroupIndex_++);
    return rg.MoveBatch();
}

bool FormatReader::HasMore() const {
    return currentRowGroupIndex_ < rowGroupMetas_.size();
}

RowGroup FormatReader::ReadRowGroup(size_t index) {
    if (!opened_)
        throw std::logic_error("Open() not called");
    if (index >= rowGroupMetas_.size())
        throw std::out_of_range("Index out of range");

    const auto& meta = rowGroupMetas_[index];
    reader_.Seek(meta.offset);

    uint32_t rowCount;
    reader_.Read(&rowCount, sizeof(rowCount));

    std::vector<Column> columns;
    columns.reserve(schema_.GetColumnCount());

    for (const auto& colSchema : schema_) {
        columns.push_back(ReadColumn(colSchema.name, colSchema.type, rowCount));
    }

    Batch batch(schema_, std::move(columns));
    return RowGroup(std::move(batch), meta);
}

const Schema& FormatReader::GetSchema() const {
    return schema_;
}

size_t FormatReader::GetRowGroupCount() const {
    return rowGroupMetas_.size();
}

uint64_t FormatReader::GetTotalRowCount() const {
    return totalRowCount_;
}

Column FormatReader::ReadColumn(const std::string& name, Types::DataType type,
                                size_t rowCount) {
    Types::AnyColumnData data;

    switch (type) {
        case Types::DataType::INT16: {
            std::vector<int16_t> vec;
            vec.reserve(rowCount);
            for (size_t i = 0; i < rowCount; ++i) {
                int16_t v;
                reader_.Read(&v, sizeof(v));
                vec.push_back(v);
            }
            data = std::move(vec);
            break;
        }
        case Types::DataType::INT32:
        case Types::DataType::DATE: {
            std::vector<int32_t> vec;
            vec.reserve(rowCount);
            for (size_t i = 0; i < rowCount; ++i) {
                int32_t v;
                reader_.Read(&v, sizeof(v));
                vec.push_back(v);
            }
            data = std::move(vec);
            break;
        }
        case Types::DataType::INT64:
        case Types::DataType::INT128:
        case Types::DataType::TIMESTAMP: {
            std::vector<int64_t> vec;
            vec.reserve(rowCount);
            for (size_t i = 0; i < rowCount; ++i) {
                int64_t v;
                reader_.Read(&v, sizeof(v));
                vec.push_back(v);
            }
            data = std::move(vec);
            break;
        }
        case Types::DataType::BOOL: {
            std::vector<bool> vec;
            vec.reserve(rowCount);
            for (size_t i = 0; i < rowCount; ++i) {
                uint8_t byte;
                reader_.Read(&byte, sizeof(byte));
                vec.push_back(byte != 0);
            }
            data = std::move(vec);
            break;
        }
        case Types::DataType::STRING: {
            std::vector<std::string> vec;
            vec.reserve(rowCount);
            for (size_t i = 0; i < rowCount; ++i) {
                vec.push_back(reader_.ReadString());
            }
            data = std::move(vec);
            break;
        }
        default:
            throw std::runtime_error("Unknown data type");
    }

    return Column(name, type, std::move(data));
}

const RowGroupMeta& FormatReader::GetRowGroupMeta(size_t index) const {
    if (index >= rowGroupMetas_.size())
        throw std::out_of_range("Index out of range");
    return rowGroupMetas_[index];
}

}  // namespace Columnar::IO