#pragma once

#include <core/column.h>
#include <core/row_group.h>
#include <core/schema.h>
#include <io/binary_io.h>

#include <string>
#include <vector>

namespace Columnar::IO {

class FormatWriter {
public:
    explicit FormatWriter(const std::string& filename);

    FormatWriter(const FormatWriter&) = delete;
    FormatWriter& operator=(const FormatWriter&) = delete;

    void Begin(const Schema& schema);
    void WriteRowGroup(const RowGroup& rowGroup);
    void End();

    size_t GetRowGroupCount() const;
    size_t GetTotalRowsWritten() const;

    ~FormatWriter();

private:
    BinaryWriter writer_;
    Schema schema_;
    std::vector<RowGroupMeta> rowGroupMetas_;

    size_t totalRowCount_ = 0;
    bool begun_ = false;
    bool ended_ = false;

    void WriteHeader();
    void WriteSchema();
    void WriteColumn(const Column& column);
    void WriteFooter();
    void FinalizeHeader();
};

}  // namespace Columnar::IO
