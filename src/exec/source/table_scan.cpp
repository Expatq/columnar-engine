#include "table_scan.h"
#include <stdexcept>
#include "util/assert.h"

namespace Columnar::Exec {

TableScan::TableScan(std::string filepath, RequiredColumns requiredCols)
    : filepath_(std::move(filepath)),
      requiredCols_(std::move(requiredCols)) {
}

void TableScan::Open() {
    reader_.emplace(filepath_);
}

bool TableScan::Next(ExecBatch& out) {
    COLUMNAR_ASSERT(reader_.has_value(), "TableScan is not opened");

    out.Reset();

    std::optional<RowGroup> rowGroup;
    switch (requiredCols_.GetMode()) {
        case RequiredColumns::Mode::All:
            rowGroup = reader_->ReadRowGroup();
            break;
        case RequiredColumns::Mode::Names:
            rowGroup = reader_->ReadRowGroup(requiredCols_.Names());
            break;
        case RequiredColumns::Mode::None:
            throw std::logic_error("cannot produce zero-column batches");
    }

    if (!rowGroup) {
        return false;
    }

    out.rowCount = rowGroup->GetRowCount();
    out.rowGroup.emplace(std::move(*rowGroup));
    return true;
}

void TableScan::Close() noexcept {
    reader_.reset();
}

const Schema& TableScan::GetSchema() const {
    COLUMNAR_ASSERT(reader_.has_value(), "TableScan is not opened");
    return reader_->GetSchema();
}

}  // namespace Columnar::Exec
