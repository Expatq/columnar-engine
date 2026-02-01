#include <io/format_writer.h>
#include <cstdint>
#include <stdexcept>
#include "core/batch.h"
#include "core/column.h"
#include "core/row_group.h"
#include "io/binary_io.h"

namespace Columnar::IO {

FormatWriter::FormatWriter(const std::string& filename)
    : writer_(filename) {}

FormatWriter::~FormatWriter() {
    if (begun_ && !ended_) {
        try {
            End();
        } catch (...) {}
    }
}

void FormatWriter::Begin(const Schema& schema) {
    if (begun_) {
        throw std::logic_error("FormatWriter::Begin() already called");
    }

    schema_ = schema;
    begun_ = true;

    WriteHeader();
    WriteSchema();
}

void FormatWriter::WriteRowGroup(const RowGroup& rowGroup) {
    if (!begun_) {
        throw std::logic_error("FormatWriter::Begin() not called");
    }
    if (ended_) {
        throw std::logic_error("FormatWriter::End() already called");
    }

    const Batch& batch = rowGroup.GetBatch();

    RowGroupMeta meta;
    meta.offset = writer_.GetPosition();

    uint32_t rowCount = static_cast<uint32_t>(batch.GetRowCount());
    writer_.Write(&rowCount, sizeof(rowCount));

    for (size_t i = 0; i < batch.GetColumnCount(); ++i) {
        WriteColumn(batch.GetColumn(i));
    }

    meta.size = writer_.GetPosition() - meta.offset;
    meta.rowCount = rowCount;

    rowGroupMetas_.push_back(meta);
    totalRowCount_ += rowCount;
}

void FormatWriter::End() {
    if (!begun_) {
        throw std::logic_error("FormatWriter::Begin() not called");
    }
    if (ended_) {
        throw std::logic_error("FormatWriter::End() already called");
    }

    WriteFooter();
    writer_.Write(kMagicBytes, kMagicSize);
    FinalizeHeader();
    writer_.Flush();
    ended_ = true;
}

size_t FormatWriter::GetRowGroupCount() const {
    return rowGroupMetas_.size();
}

size_t FormatWriter::GetTotalRowsWritten() const {
    return totalRowCount_;
}

void FormatWriter::WriteHeader() {
    uint32_t columnCount = static_cast<uint32_t>(schema_.GetColumnCount());
    uint32_t rowGroupCount = 0;
    uint64_t totalRowCount = 0;
    uint64_t schemaOffset = kHeaderSize;
    uint64_t footerOffset = 0;

    writer_.Write(&columnCount, sizeof(columnCount));
    writer_.Write(&rowGroupCount, sizeof(rowGroupCount));
    writer_.Write(&totalRowCount, sizeof(totalRowCount));
    writer_.Write(&schemaOffset, sizeof(schemaOffset));
    writer_.Write(&footerOffset, sizeof(footerOffset));

    char reserved[32] = {};
    writer_.Write(reserved, sizeof(reserved));
}

void FormatWriter::WriteSchema() {
    for (const auto& col : schema_) {
        uint8_t type = static_cast<uint8_t>(col.type);
        writer_.Write(&type, sizeof(type));
        writer_.WriteString(col.name);
    }
}

void FormatWriter::WriteColumn(const Column& column) {
    const auto& data = column.GetData();

    std::visit(Types::overloaded{[this](const std::vector<int16_t>& vec) {
                                     for (int16_t v : vec) {
                                         writer_.Write(&v, sizeof(v));
                                     }
                                 },
                                 [this](const std::vector<int32_t>& vec) {
                                     for (int32_t v : vec) {
                                         writer_.Write(&v, sizeof(v));
                                     }
                                 },
                                 [this](const std::vector<int64_t>& vec) {
                                     for (int64_t v : vec) {
                                         writer_.Write(&v, sizeof(v));
                                     }
                                 },
                                 [this](const std::vector<bool>& vec) {
                                     for (bool v : vec) {
                                         uint8_t byte = v ? 1 : 0;
                                         writer_.Write(&byte, sizeof(byte));
                                     }
                                 },
                                 [this](const std::vector<std::string>& vec) {
                                     for (const auto& s : vec) {
                                         writer_.WriteString(s);
                                     }
                                 }},
               data);
}

void FormatWriter::WriteFooter() {
    for (const auto& meta : rowGroupMetas_) {
        writer_.Write(&meta.offset, sizeof(meta.offset));
        writer_.Write(&meta.size, sizeof(meta.size));
        writer_.Write(&meta.rowCount, sizeof(meta.rowCount));
    }
}

void FormatWriter::FinalizeHeader() {
    size_t currentPos = writer_.GetPosition();
    size_t footerSize = rowGroupMetas_.size() * RowGroupMeta::kSerializedSize;
    uint64_t footerOffset = currentPos - kMagicSize - footerSize;

    writer_.Seek(4);
    uint32_t rowGroupCount = static_cast<uint32_t>(rowGroupMetas_.size());
    writer_.Write(&rowGroupCount, sizeof(rowGroupCount));
    writer_.Write(&totalRowCount_, sizeof(totalRowCount_));

    writer_.Seek(24);
    writer_.Write(&footerOffset, sizeof(footerOffset));

    writer_.Seek(currentPos);
}

}  // namespace Columnar::IO