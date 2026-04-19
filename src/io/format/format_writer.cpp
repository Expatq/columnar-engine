#include <io/binary/binary_io.h>
#include <io/format/format_writer.h>

#include <util/assert.h>

#include <core/column.h>
#include <core/row_group.h>

#include <cstdint>

namespace Columnar::IO {

FormatWriter::FormatWriter(const std::string& filename)
    : writer_(filename) {}

FormatWriter::~FormatWriter() {
    if (begun_ && !ended_) {
        try {
            End();
        } catch (...) {
        }
    }
}

void FormatWriter::Begin(const Schema& schema) {
    COLUMNAR_ASSERT(!begun_, "FormatWriter::Begin already called");
    schema_ = schema;
    begun_ = true;
    WriteHeader();
    WriteSchema();
}

void FormatWriter::WriteRowGroup(const RowGroup& rg) {
    COLUMNAR_ASSERT(begun_, "WriteRowGroup called before Begin");
    COLUMNAR_ASSERT(!ended_, "WriteRowGroup called after End");
    COLUMNAR_ASSERT(rg.GetColumnCount() == schema_.GetColumnCount(),
                    "WriteRowGroup: column count mismatch");

    const uint64_t rgStart = static_cast<uint64_t>(writer_.GetPosition());
    const int64_t rowCount = static_cast<int64_t>(rg.GetRowCount());
    const size_t nCols = rg.GetColumnCount();

    writer_.Write(&rowCount, sizeof(rowCount));

    const size_t offsetsPos = writer_.GetPosition();
    for (size_t i = 0; i < nCols; ++i) {
        int64_t placeholder = 0;
        writer_.Write(&placeholder, sizeof(placeholder));
    }

    std::vector<int64_t> colOffsets;
    colOffsets.reserve(nCols);
    for (size_t i = 0; i < nCols; ++i) {
        colOffsets.push_back(static_cast<int64_t>(writer_.GetPosition()) -
                             static_cast<int64_t>(rgStart));
        WriteColumn(rg.GetColumn(i));
    }

    const size_t endPos = writer_.GetPosition();

    writer_.Seek(offsetsPos);
    for (int64_t off : colOffsets) {
        writer_.Write(&off, sizeof(off));
    }
    writer_.Seek(endPos);

    rgMetas_.push_back({rgStart, static_cast<uint32_t>(rowCount)});
    totalRowCount_ += static_cast<size_t>(rowCount);
}

void FormatWriter::End() {
    COLUMNAR_ASSERT(begun_, "End called before Begin");
    COLUMNAR_ASSERT(!ended_, "End called twice");
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

    writer_.Write(&colCount, sizeof(colCount));
    writer_.Write(&rgCount, sizeof(rgCount));
    writer_.Write(&totalRowCount, sizeof(totalRowCount));
    writer_.Write(&schemaOffset, sizeof(schemaOffset));
    writer_.Write(&footerOffset, sizeof(footerOffset));

    char reserved[32] = {};
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
                       for (int16_t x : v) {
                           writer_.Write(&x, sizeof(x));
                       }
                   },
                   [this](const std::vector<int32_t>& v) {
                       for (int32_t x : v) {
                           writer_.Write(&x, sizeof(x));
                       }
                   },
                   [this](const std::vector<int64_t>& v) {
                       for (int64_t x : v) {
                           writer_.Write(&x, sizeof(x));
                       }
                   },
                   [this](const std::vector<bool>& v) {
                       for (bool b : v) {
                           uint8_t byte = b ? 1 : 0;
                           writer_.Write(&byte, sizeof(byte));
                       }
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
    for (const auto& m : rgMetas_) {
        writer_.Write(&m.offset, sizeof(m.offset));
    }
    for (const auto& m : rgMetas_) {
        writer_.Write(&m.rowCount, sizeof(m.rowCount));
    }
}

void FormatWriter::FinalizeHeader() {
    const size_t cur = writer_.GetPosition();
    const size_t footerBodySize = sizeof(uint32_t) + rgMetas_.size() * (sizeof(uint64_t) + sizeof(uint32_t));
    const uint64_t footerOffset = static_cast<uint64_t>(cur - kMagicSize - footerBodySize);
    const uint32_t rgCount = static_cast<uint32_t>(rgMetas_.size());
    const uint64_t totalRowCount = static_cast<uint64_t>(totalRowCount_);

    writer_.Seek(4);
    writer_.Write(&rgCount, sizeof(rgCount));
    writer_.Write(&totalRowCount, sizeof(totalRowCount));
    writer_.Seek(24);
    writer_.Write(&footerOffset, sizeof(footerOffset));
    writer_.Seek(cur);
}

}  // namespace Columnar::IO
