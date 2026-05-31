#include <io/format/bitpack.h>
#include <io/format/format_defs.h>
#include <io/format/format_writer.h>

#include <core/col_stats.h>
#include <core/column.h>
#include <core/row_group.h>
#include <core/row_group_meta.h>
#include <core/types.h>

#include <util/assert.h>
#include <util/byte_buffer.h>
#include <util/int128.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Columnar::IO {

namespace {

ColStats EncodeColumnToBuffer(const Column& col, ByteBuffer& buf) {
    ColStats stats;
    std::visit(
        Types::overloaded{
            [&](const std::vector<int16_t>& v) {
                if (v.empty()) {
                    buf.Append(kEncRaw);
                    return;
                }
                const auto [minIt, maxIt] = std::minmax_element(v.begin(), v.end());
                stats.minVal = *minIt;
                stats.maxVal = *maxIt;
                const int64_t minVal = *minIt;
                const uint8_t needed = MinBits(static_cast<uint64_t>(*maxIt - *minIt));
                if (ShouldBitpack<int16_t>(needed)) {
                    buf.Append(kEncBitpack);
                    buf.Append(minVal);
                    buf.Append(needed);
                    std::vector<uint32_t> shifted(v.size());
                    for (size_t i = 0; i < v.size(); ++i)
                        shifted[i] = static_cast<uint32_t>(static_cast<int64_t>(v[i]) - minVal);
                    std::vector<uint8_t> packed(PackedBytes(v.size(), needed));
                    BitpackEncode(shifted.data(), v.size(), needed, packed.data());
                    buf.Append(packed.data(), packed.size());
                } else {
                    buf.Append(kEncRaw);
                    buf.Append(v.data(), v.size() * sizeof(int16_t));
                }
            },
            [&](const std::vector<int32_t>& v) {
                if (v.empty()) {
                    buf.Append(kEncRaw);
                    return;
                }
                const auto [minIt, maxIt] = std::minmax_element(v.begin(), v.end());
                stats.minVal = *minIt;
                stats.maxVal = *maxIt;
                const int64_t minVal = *minIt;
                const uint8_t needed = MinBits(static_cast<uint64_t>(*maxIt - *minIt));
                if (ShouldBitpack<int32_t>(needed)) {
                    buf.Append(kEncBitpack);
                    buf.Append(minVal);
                    buf.Append(needed);
                    std::vector<uint32_t> shifted(v.size());
                    for (size_t i = 0; i < v.size(); ++i)
                        shifted[i] = static_cast<uint32_t>(static_cast<int64_t>(v[i]) - minVal);
                    std::vector<uint8_t> packed(PackedBytes(v.size(), needed));
                    BitpackEncode(shifted.data(), v.size(), needed, packed.data());
                    buf.Append(packed.data(), packed.size());
                } else {
                    buf.Append(kEncRaw);
                    buf.Append(v.data(), v.size() * sizeof(int32_t));
                }
            },
            [&](const std::vector<int64_t>& v) {
                buf.Append(kEncRaw);
                if (v.empty())
                    return;
                const auto [minIt, maxIt] = std::minmax_element(v.begin(), v.end());
                stats.minVal = *minIt;
                stats.maxVal = *maxIt;
                buf.Append(v.data(), v.size() * sizeof(int64_t));
            },
            [&](const std::vector<Int128>& v) {
                buf.Append(kEncRaw);
                if (v.empty())
                    return;
                const auto [minIt, maxIt] = std::minmax_element(v.begin(), v.end());
                stats.minVal = *minIt;
                stats.maxVal = *maxIt;
                buf.Append(v.data(), v.size() * sizeof(Int128));
            },
            [&](const std::vector<uint8_t>& v) {
                buf.Append(kEncRaw);
                if (v.empty())
                    return;
                const auto [minIt, maxIt] = std::minmax_element(v.begin(), v.end());
                stats.minVal = *minIt;
                stats.maxVal = *maxIt;
                buf.Append(v.data(), v.size());
            },
            [&](const std::vector<std::string>& v) {
                uint64_t totalChars = 0;
                for (const auto& s : v) totalChars += s.size();
                buf.Append(totalChars);

                uint32_t offset = 0;
                for (const auto& s : v) {
                    buf.Append(offset);
                    offset += static_cast<uint32_t>(s.size());
                }
                buf.Append(offset);

                for (const auto& s : v)
                    if (!s.empty())
                        buf.Append(s.data(), s.size());
            },
        },
        col.GetData());

    return stats;
}

}  // namespace

FormatWriter::EncodedRowGroup FormatWriter::EncodeRowGroup(const RowGroup& rg) {
    const size_t rowCount = rg.GetRowCount();
    const size_t colCount = rg.GetColumnCount();

    ByteBuffer buf;
    buf.reserve(rowCount * colCount * sizeof(int32_t));

    buf.Append(static_cast<int64_t>(rowCount));

    const size_t colOffsetTablePos = buf.size();
    for (size_t i = 0; i < colCount; ++i)
        buf.Append(static_cast<uint64_t>(0));

    std::vector<uint64_t> colOffsets(colCount);
    std::vector<ColStats> stats(colCount);

    for (size_t i = 0; i < colCount; ++i) {
        colOffsets[i] = buf.size();
        stats[i] = EncodeColumnToBuffer(rg.GetColumn(i), buf);
    }

    for (size_t i = 0; i < colCount; ++i)
        buf.PatchAt(colOffsetTablePos + i * sizeof(uint64_t), colOffsets[i]);

    return {std::move(buf.data), std::move(stats), static_cast<uint32_t>(rowCount)};
}

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

void FormatWriter::AppendBlob(const RowGroup& rg) {
    COLUMNAR_ASSERT(begun_ && !ended_, "not in progress");
    COLUMNAR_ASSERT(rg.GetColumnCount() == schema_.GetColumnCount(), "column count mismatch");
    for (size_t i = 0; i < rg.GetColumnCount(); ++i)
        COLUMNAR_ASSERT(rg.GetColumn(i).GetType() == schema_.GetColumn(i).physical,
                        "physical type mismatch");
    WriteEncoded(EncodeRowGroup(rg));
}

void FormatWriter::AppendEncoded(EncodedRowGroup encRg) {
    COLUMNAR_ASSERT(begun_ && !ended_, "not in progress");
    WriteEncoded(std::move(encRg));
}

void FormatWriter::WriteEncoded(EncodedRowGroup enc) {
    const uint64_t rgOffset = static_cast<uint64_t>(writer_.GetPosition());
    writer_.Write(enc.blob.data(), enc.blob.size());
    rgMetas_.push_back(RowGroupMeta{rgOffset, enc.rowCount});
    totalRowCount_ += enc.rowCount;
    allStats_.data.push_back(std::move(enc.stats));
}

void FormatWriter::End() {
    COLUMNAR_ASSERT(begun_, "called before Begin");
    COLUMNAR_ASSERT(!ended_, "called twice");

    const uint64_t footerOffset = static_cast<uint64_t>(writer_.GetPosition());
    const uint32_t rgCount = static_cast<uint32_t>(rgMetas_.size());

    writer_.Write(&rgCount, sizeof(rgCount));
    for (const auto& m : rgMetas_) writer_.Write(&m.offset, sizeof(m.offset));
    for (const auto& m : rgMetas_) writer_.Write(&m.rowCount, sizeof(m.rowCount));

    const uint8_t hasStats = allStats_.empty() ? 0u : 1u;
    writer_.Write(&hasStats, sizeof(hasStats));

    if (hasStats) {
        const size_t colCount = allStats_.data[0].size();
        std::vector<ColStats> flat;
        flat.reserve(rgCount * colCount);
        for (const auto& rgStats : allStats_.data)
            flat.insert(flat.end(), rgStats.begin(), rgStats.end());
        writer_.Write(flat.data(), flat.size() * sizeof(ColStats));
    }

    writer_.Write(kMagicBytes, kMagicSize);
    FinalizeHeader(footerOffset, rgCount);
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
    const uint32_t colCount = static_cast<uint32_t>(schema_.GetColumnCount());
    const uint32_t rgCount = 0;
    const uint64_t totalRowCount = 0;
    const uint64_t schemaOffset = kHeaderSize;
    const uint64_t footerOffset = 0;
    char reserved[kHeaderReservedSize] = {};

    writer_.Write(&colCount, sizeof(colCount));
    writer_.Write(&rgCount, sizeof(rgCount));
    writer_.Write(&totalRowCount, sizeof(totalRowCount));
    writer_.Write(&schemaOffset, sizeof(schemaOffset));
    writer_.Write(&footerOffset, sizeof(footerOffset));
    writer_.Write(reserved, sizeof(reserved));
}

void FormatWriter::WriteSchema() {
    for (const auto& col : schema_) {
        const uint8_t type = static_cast<uint8_t>(col.logical);
        writer_.Write(&type, sizeof(type));
        writer_.WriteString(col.name);
    }
}

void FormatWriter::FinalizeHeader(uint64_t footerOffset, uint32_t rgCount) {
    const size_t endPos = writer_.GetPosition();
    const uint64_t totalRowCount = static_cast<uint64_t>(totalRowCount_);
    writer_.Seek(kHeaderOffsetRgCount);
    writer_.Write(&rgCount, sizeof(rgCount));
    writer_.Write(&totalRowCount, sizeof(totalRowCount));
    writer_.Seek(kHeaderOffsetFooterOffset);
    writer_.Write(&footerOffset, sizeof(footerOffset));
    writer_.Seek(endPos);
}

}  // namespace Columnar::IO
