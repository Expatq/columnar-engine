#include <io/format/bitpack.h>
#include <io/format/format_defs.h>
#include <io/format/format_writer.h>

#include <util/assert.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "core/column.h"
#include "core/row_group.h"
#include "core/row_group_meta.h"
#include "core/types.h"
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
    COLUMNAR_ASSERT(rg.GetColumnCount() == schema_.GetColumnCount(), "column count mismatch");

    for (size_t i = 0; i < rg.GetColumnCount(); ++i)
        COLUMNAR_ASSERT(rg.GetColumn(i).GetType() == schema_.GetColumn(i).physical,
                        "physical type mismatch");

    const size_t rgOffset = writer_.GetPosition();
    const size_t rowCount = rg.GetRowCount();
    const size_t columnCount = rg.GetColumnCount();

    writer_.Write(&rowCount, sizeof(rowCount));

    const size_t colOffsetTablePos = writer_.GetPosition();
    for (size_t i = 0; i < columnCount; ++i) {
        uint64_t placeholder = 0;
        writer_.Write(&placeholder, sizeof(placeholder));
    }

    std::vector<size_t> colOffsets;
    colOffsets.reserve(columnCount);

    for (size_t i = 0; i < columnCount; ++i) {
        colOffsets.push_back(writer_.GetPosition() - rgOffset);
        WriteColumn(rg.GetColumn(i));
    }

    const size_t afterColumnsPos = writer_.GetPosition();

    writer_.Seek(colOffsetTablePos);
    for (size_t offset : colOffsets) {
        const uint64_t off64 = static_cast<uint64_t>(offset);
        writer_.Write(&off64, sizeof(off64));
    }
    writer_.Seek(afterColumnsPos);

    rgMetas_.push_back(RowGroupMeta{
        static_cast<uint64_t>(rgOffset),
        static_cast<uint32_t>(rowCount)});
    totalRowCount_ += rowCount;

    CollectStats(rg);
}

void FormatWriter::End() {
    COLUMNAR_ASSERT(begun_, "called before Begin");
    COLUMNAR_ASSERT(!ended_, "called twice");

    const uint64_t footerOffset = static_cast<uint64_t>(writer_.GetPosition());
    const uint32_t rgCount = static_cast<uint32_t>(rgMetas_.size());

    writer_.Write(&rgCount, sizeof(rgCount));

    for (const auto& m : rgMetas_)
        writer_.Write(&m.offset, sizeof(m.offset));

    for (const auto& m : rgMetas_)
        writer_.Write(&m.rowCount, sizeof(m.rowCount));

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
        const uint8_t type = static_cast<uint8_t>(col.logical);
        writer_.Write(&type, sizeof(type));
        writer_.WriteString(col.name);
    }
}

void FormatWriter::WriteColumn(const Column& col) {
    std::visit(Types::overloaded{
                   [this](const std::vector<int16_t>& v) {
                       if (v.empty()) {
                           const uint8_t enc = 0x00;
                           writer_.Write(&enc, sizeof(enc));
                           return;
                       }
                       const auto [minIt, maxIt] = std::minmax_element(v.begin(), v.end());
                       const int64_t minVal = *minIt;
                       const int64_t maxVal = *maxIt;
                       const uint64_t range = static_cast<uint64_t>(maxVal - minVal);
                       const uint8_t needed = MinBits(range);
                       if (needed * 4 < sizeof(int16_t) * 8 * 3) {
                           const uint8_t enc = 0x01;
                           writer_.Write(&enc, sizeof(enc));
                           writer_.Write(&minVal, sizeof(minVal));
                           writer_.Write(&needed, sizeof(needed));
                           std::vector<uint32_t> shifted(v.size());
                           for (size_t i = 0; i < v.size(); ++i)
                               shifted[i] = static_cast<uint32_t>(static_cast<int64_t>(v[i]) - minVal);
                           std::vector<uint8_t> packed((v.size() * needed + 7) / 8);
                           BitpackEncode(shifted.data(), v.size(), needed, packed.data());
                           writer_.Write(packed.data(), packed.size());
                       } else {
                           const uint8_t enc = 0x00;
                           writer_.Write(&enc, sizeof(enc));
                           writer_.Write(v.data(), v.size() * sizeof(v[0]));
                       }
                   },
                   [this](const std::vector<int32_t>& v) {
                       if (v.empty()) {
                           const uint8_t enc = 0x00;
                           writer_.Write(&enc, sizeof(enc));
                           return;
                       }
                       const auto [minIt, maxIt] = std::minmax_element(v.begin(), v.end());
                       const int64_t minVal = *minIt;
                       const int64_t maxVal = *maxIt;
                       const uint64_t range = static_cast<uint64_t>(maxVal - minVal);
                       const uint8_t needed = MinBits(range);
                       if (needed * 4 < sizeof(int32_t) * 8 * 3) {
                           const uint8_t enc = 0x01;
                           writer_.Write(&enc, sizeof(enc));
                           writer_.Write(&minVal, sizeof(minVal));
                           writer_.Write(&needed, sizeof(needed));
                           std::vector<uint32_t> shifted(v.size());
                           for (size_t i = 0; i < v.size(); ++i)
                               shifted[i] = static_cast<uint32_t>(static_cast<int64_t>(v[i]) - minVal);
                           std::vector<uint8_t> packed((v.size() * needed + 7) / 8);
                           BitpackEncode(shifted.data(), v.size(), needed, packed.data());
                           writer_.Write(packed.data(), packed.size());
                       } else {
                           const uint8_t enc = 0x00;
                           writer_.Write(&enc, sizeof(enc));
                           writer_.Write(v.data(), v.size() * sizeof(v[0]));
                       }
                   },
                   [this](const std::vector<int64_t>& v) {
                       const uint8_t enc = 0x00;
                       writer_.Write(&enc, sizeof(enc));
                       if (!v.empty())
                           writer_.Write(v.data(), v.size() * sizeof(v[0]));
                   },
                   [this](const std::vector<Int128>& v) {
                       const uint8_t enc = 0x00;
                       writer_.Write(&enc, sizeof(enc));
                       if (!v.empty())
                           writer_.Write(v.data(), v.size() * sizeof(v[0]));
                   },
                   [this](const std::vector<uint8_t>& v) {
                       const uint8_t enc = 0x00;
                       writer_.Write(&enc, sizeof(enc));
                       if (!v.empty())
                           writer_.Write(v.data(), v.size() * sizeof(v[0]));
                   },
                   [this](const std::vector<std::string>& v) {
                       uint64_t totalChars = 0;
                       for (const auto& s : v) totalChars += s.size();
                       writer_.Write(&totalChars, sizeof(totalChars));

                       uint32_t offset = 0;
                       for (const auto& s : v) {
                           writer_.Write(&offset, sizeof(offset));
                           offset += static_cast<uint32_t>(s.size());
                       }
                       writer_.Write(&offset, sizeof(offset));

                       for (const auto& s : v)
                           if (!s.empty())
                               writer_.Write(s.data(), s.size());
                   },
               },
               col.GetData());
}

void FormatWriter::CollectStats(const RowGroup& rg) {
    auto& rgStats = allStats_.data.emplace_back(rg.GetColumnCount());
    for (size_t c = 0; c < rg.GetColumnCount(); ++c) {
        auto& s = rgStats[c];
        std::visit(Types::overloaded{
                       [&](const std::vector<int16_t>& v) {
                           for (int16_t x : v) {
                               const int64_t val = x;
                               if (val < s.minVal)
                                   s.minVal = val;
                               if (val > s.maxVal)
                                   s.maxVal = val;
                           }
                       },
                       [&](const std::vector<int32_t>& v) {
                           for (int32_t x : v) {
                               const int64_t val = x;
                               if (val < s.minVal)
                                   s.minVal = val;
                               if (val > s.maxVal)
                                   s.maxVal = val;
                           }
                       },
                       [&](const std::vector<int64_t>& v) {
                           for (int64_t x : v) {
                               if (x < s.minVal)
                                   s.minVal = x;
                               if (x > s.maxVal)
                                   s.maxVal = x;
                           }
                       },
                       [&](const std::vector<uint8_t>& v) {
                           for (uint8_t x : v) {
                               const int64_t val = x;
                               if (val < s.minVal)
                                   s.minVal = val;
                               if (val > s.maxVal)
                                   s.maxVal = val;
                           }
                       },
                       [&](const std::vector<Int128>& v) {
                           for (Int128 x : v) {
                               if (x < s.minVal)
                                   s.minVal = x;
                               if (x > s.maxVal)
                                   s.maxVal = x;
                           }
                       },
                       [](const std::vector<std::string>&) {},
                   },
                   rg.GetColumn(c).GetData());
    }
}

void FormatWriter::FinalizeHeader(uint64_t footerOffset, uint32_t rgCount) {
    const size_t endPos = writer_.GetPosition();
    const uint64_t totalRowCount = static_cast<uint64_t>(totalRowCount_);

    writer_.Seek(4);
    writer_.Write(&rgCount, sizeof(rgCount));
    writer_.Write(&totalRowCount, sizeof(totalRowCount));
    writer_.Seek(24);
    writer_.Write(&footerOffset, sizeof(footerOffset));
    writer_.Seek(endPos);
}

}  // namespace Columnar::IO
