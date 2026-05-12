#include <io/format/format_defs.h>
#include <io/format/format_writer.h>

#include <util/assert.h>

#include <core/column.h>
#include <core/row_group.h>

#include <cstdint>
#include "core/row_group_meta.h"
#include "util/int128.h"

namespace Columnar::IO {

FormatWriter::FormatWriter(const std::string& filename)
    : writer_(filename) {
}

FormatWriter::~FormatWriter() {
    if (begun_ && !ended_) {
        try {
            End();
        } catch (...) {
        }
    }
}

void FormatWriter::Begin(const Schema& schema) {
    COLUMNAR_ASSERT(!begun_, "already called");
    COLUMNAR_ASSERT(schema.GetColumnCount() > 0, "schema cannot be empty");

    schema_ = schema;
    begun_ = true;

    WriteHeader();
    WriteSchema();
}

void FormatWriter::WriteRowGroup(const RowGroup& rg) {
    COLUMNAR_ASSERT(begun_, "called before Begin");
    COLUMNAR_ASSERT(!ended_, "called after End");
    COLUMNAR_ASSERT(rg.GetColumnCount() == schema_.GetColumnCount(),
                    "column count mismatch");

    for (size_t i = 0; i < rg.GetColumnCount(); ++i) {
        COLUMNAR_ASSERT(rg.GetColumn(i).GetType() == schema_.GetColumn(i).physical, "physical type mismatch");
    }

    const size_t rgStart = writer_.GetPosition();
    const size_t rowCount = rg.GetRowCount();
    const size_t colsCount = rg.GetColumnCount();

    writer_.Write(&rowCount, sizeof(rowCount));

    const size_t offsetPos = writer_.GetPosition();
    for (size_t i = 0; i < colsCount; ++i) {
        uint64_t dummy = 0;
        writer_.Write(&dummy, sizeof(dummy));
    }

    std::vector<size_t> colOffsets;
    colOffsets.reserve(colsCount);

    for (size_t i = 0; i < colsCount; ++i) {
        colOffsets.push_back(writer_.GetPosition() - rgStart);
        WriteColumn(rg.GetColumn(i));
    }

    const size_t endPos = writer_.GetPosition();

    writer_.Seek(offsetPos);
    for (uint64_t offset : colOffsets) {
        writer_.Write(&offset, sizeof(offset));
    }
    writer_.Seek(endPos);

    rgMetas_.push_back(RowGroupMeta{rgStart, static_cast<uint32_t>(rg.GetRowCount())});
    totalRowCount_ += rg.GetRowCount();
}

void FormatWriter::End() {
    COLUMNAR_ASSERT(begun_, "called before Begin");
    COLUMNAR_ASSERT(!ended_, "called twice");

    WriteFooter();
    writer_.Write(kMagicBytes, kMagicSize);
    FinalizeHeader();
    writer_.Flush();

    ended_ = true;
}

size_t FormatWriter::GetRowGroupCount() const {
    return rgMetas_.size();
}

size_t FormatWriter::GetTotalRowsWritten() const {
    return totalRowCount_;
}

void FormatWriter::WriteHeader() {
    uint32_t colCount = static_cast<uint32_t>(schema_.GetColumnCount());
    uint32_t rgCount = 0;
    uint64_t totalRowCount = 0;
    uint64_t schemaOffset = kHeaderSize;
    uint64_t footerOffset = 0;
    char reserved[32] = {};

    writer_.Write(&colCount, sizeof(colCount));
    writer_.Write(&rgCount, sizeof(rgCount));
    writer_.Write(&totalRowCount, sizeof(totalRowCount));
    writer_.Write(&schemaOffset, sizeof(schemaOffset));
    writer_.Write(&footerOffset, sizeof(footerOffset));
    writer_.Write(reserved, sizeof(reserved));
}

void FormatWriter::WriteSchema() {
    for (const auto& col : schema_) {
        uint8_t type = static_cast<uint8_t>(col.logical);
        writer_.Write(&type, sizeof(type));
        writer_.WriteString(col.name);
    }
}

void FormatWriter::WriteColumn(const Column& col) {
    std::visit(Types::overloaded{
                   [this](const std::vector<int16_t>& v) {
                       if (!v.empty())
                           writer_.Write(v.data(), v.size() * sizeof(v[0]));
                   },
                   [this](const std::vector<int32_t>& v) {
                       if (!v.empty())
                           writer_.Write(v.data(), v.size() * sizeof(v[0]));
                   },
                   [this](const std::vector<int64_t>& v) {
                       if (!v.empty())
                           writer_.Write(v.data(), v.size() * sizeof(v[0]));
                   },
                   [this](const std::vector<Int128>& v) {
                       if (!v.empty())
                           writer_.Write(v.data(), v.size() * sizeof(v[0]));
                   },
                   [this](const std::vector<uint8_t>& v) {
                       if (!v.empty())
                           writer_.Write(v.data(), v.size() * sizeof(v[0]));
                   },
                   [this](const std::vector<std::string>& v) {
                       for (const auto& s : v) {
                           writer_.WriteString(s);
                       }
                   },
               },
               col.GetData());
}

void FormatWriter::WriteFooter() {
    uint32_t rgCount = static_cast<uint32_t>(rgMetas_.size());
    writer_.Write(&rgCount, sizeof(rgCount));

    for (const auto& meta : rgMetas_) {
        writer_.Write(&meta.offset, sizeof(meta.offset));
    }

    for (const auto& meta : rgMetas_) {
        writer_.Write(&meta.rowCount, sizeof(meta.rowCount));
    }
}

void FormatWriter::FinalizeHeader() {
    const size_t endPos = writer_.GetPosition();
    const size_t footerBodySize = sizeof(uint32_t) + rgMetas_.size() * (sizeof(uint64_t) + sizeof(uint32_t));
    const uint64_t footerOffset = static_cast<uint64_t>(endPos - kMagicSize - footerBodySize);
    const uint32_t rgCount = static_cast<uint32_t>(rgMetas_.size());
    const uint64_t totalRowCount = static_cast<uint64_t>(totalRowCount_);

    writer_.Seek(4);
    writer_.Write(&rgCount, sizeof(rgCount));
    writer_.Write(&totalRowCount, sizeof(totalRowCount));

    writer_.Seek(24);
    writer_.Write(&footerOffset, sizeof(footerOffset));

    writer_.Seek(endPos);
}

}  // namespace Columnar::IO
