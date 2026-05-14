#pragma once

#include <core/column.h>
#include <core/row_group.h>
#include <core/row_group_meta.h>
#include <core/schema.h>
#include <io/binary/file_writer.h>
#include <io/format/stats_block.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Columnar::IO {

class FormatWriter {
public:
    explicit FormatWriter(const std::string& filename);
    ~FormatWriter();

    FormatWriter(const FormatWriter&) = delete;
    FormatWriter& operator=(const FormatWriter&) = delete;

    FormatWriter(FormatWriter&&) noexcept = default;
    FormatWriter& operator=(FormatWriter&&) noexcept = default;

    void Begin(const Schema& schema);
    void WriteRowGroup(const RowGroup& rg);
    void End();

    size_t GetRowGroupCount() const;
    size_t GetTotalRowsWritten() const;

private:
    FileWriter writer_;
    Schema schema_;

    std::vector<RowGroupMeta> rgMetas_;
    StatsBlock allStats_;

    size_t totalRowCount_ = 0;
    bool begun_ = false;
    bool ended_ = false;

    void WriteHeader();
    void WriteSchema();
    void WriteColumn(const Column& col);
    void CollectStats(const RowGroup& rg);
    void FinalizeHeader(uint64_t footerOffset, uint32_t rgCount);
};

}  // namespace Columnar::IO
