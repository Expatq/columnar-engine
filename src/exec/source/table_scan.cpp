#include "table_scan.h"

#include <util/assert.h>

#include <stdexcept>

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

    while (reader_->HasMore()) {
        const size_t idx = reader_->CurrentRowGroupIndex();
        if (ShouldSkip(idx)) {
            reader_->SkipRowGroup();
            continue;
        }

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
        out.rowGroup = std::make_shared<RowGroup>(std::move(*rowGroup));
        return true;
    }

    return false;
}

void TableScan::AddRangePredicate(size_t colIdx, int64_t lo, int64_t hi) {
    rangePredicates_.push_back({colIdx, lo, hi});
}

void TableScan::AddEqualityPredicate(size_t colIdx, int64_t value) {
    rangePredicates_.push_back({colIdx, value, value});
}

bool TableScan::ShouldSkip(size_t rgIdx) const {
    if (rangePredicates_.empty()) {
        return false;
    }
    
    for (const auto& predicate : rangePredicates_) {
        const ColStats* stats = reader_->GetStats(rgIdx, predicate.colIdx);
        if (stats && !stats->MayContain(predicate.lo, predicate.hi)) {
            return true;
        }
    }
    return false;
}

void TableScan::Close() noexcept {
    reader_.reset();
}

const Schema& TableScan::GetSchema() const {
    COLUMNAR_ASSERT(reader_.has_value(), "TableScan is not opened");
    return reader_->GetSchema();
}

}  // namespace Columnar::Exec
