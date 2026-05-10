#include "sort.h"

#include <exec/result_format/row_group_builder.h>

#include <util/assert.h>

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include "core/row_group.h"

namespace Columnar::Exec {

Sort::Sort(std::unique_ptr<IOperator> child, std::vector<SortKey> keys)
    : child_(std::move(child)),
      keys_(std::move(keys)) {
    if (!child_ || keys_.empty())
        throw std::invalid_argument("Sort: invalid arguments");
}

void Sort::Open() {
    child_->Open();
    columns_.clear();
    keyColIndices_.clear();
    rowCount_ = 0;
    produced_ = false;
}

bool Sort::Next(ExecBatch& out) {
    if (produced_) {
        return false;
    }

    while (child_->Next(input_)) {
        AppendBatch(input_);
    }

    std::vector<size_t> indices(rowCount_);
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [this](size_t lhsRowId, size_t rhsRowId) {
        return IsLess(lhsRowId, rhsRowId);
    });

    out.Reset();
    out.rowGroup = std::make_shared<RowGroup>(BuildOutput(indices));
    out.rowCount = rowCount_;
    produced_ = true;
    return rowCount_ > 0;
}

void Sort::Close() noexcept {
    child_->Close();
    columns_.clear();
    keyColIndices_.clear();
    rowCount_ = 0;
    produced_ = false;
}

bool Sort::IsLess(size_t lhsRowId, size_t rhsRowId) const {
    for (size_t i = 0; i < keys_.size(); ++i) {
        auto [eq, less] = std::visit([&](const auto& vec) -> std::pair<bool, bool> {
            if (vec[lhsRowId] == vec[rhsRowId]) {
                return {true, false};
            }
            return {false, vec[lhsRowId] < vec[rhsRowId]};
        },
                                     columns_[keyColIndices_[i]]);
        if (eq) {
            continue;
        }
        return keys_[i].descending ? !less : less;
    }
    return false;
}

void Sort::AppendBatch(const ExecBatch& batch) {
    if (!batch.rowGroup || batch.Empty()) {
        return;
    }

    const RowGroup& rowGroup = *batch.rowGroup;
    if (columns_.empty()) {
        schema_ = rowGroup.GetSchema();
        for (size_t colIdx = 0; colIdx < schema_.GetColumnCount(); ++colIdx) {
            columns_.push_back(Types::CreateEmptyColumnData(schema_.GetColumn(colIdx).physical));
        }
        for (const auto& key : keys_) {
            const auto cols = key.expr->RequiredColumns();
            COLUMNAR_ASSERT(!cols.empty(), "sort key requires a column reference");
            const auto idx = schema_.FindColumn(cols[0]);
            COLUMNAR_ASSERT(idx.has_value(), "sort key column not found in schema");
            keyColIndices_.push_back(*idx);
        }
    }

    if (batch.has_selection) {
        for (size_t colIdx = 0; colIdx < rowGroup.GetColumnCount(); ++colIdx) {
            std::visit([&](auto& dst) {
                using T = typename std::decay_t<decltype(dst)>::value_type;
                const auto& src = std::get<std::vector<T>>(rowGroup.GetColumn(colIdx).GetData());
                for (RowId rowId : batch.selection.Rows())
                    dst.push_back(src[rowId]);
            },
                       columns_[colIdx]);
        }
        rowCount_ += batch.selection.Size();
    } else {
        for (size_t colIdx = 0; colIdx < rowGroup.GetColumnCount(); ++colIdx) {
            std::visit([&](auto& dst) {
                using T = typename std::decay_t<decltype(dst)>::value_type;
                const auto& src = std::get<std::vector<T>>(rowGroup.GetColumn(colIdx).GetData());
                dst.insert(dst.end(), src.begin(), src.begin() + batch.rowCount);
            },
                       columns_[colIdx]);
        }
        rowCount_ += batch.rowCount;
    }
}

RowGroup Sort::BuildOutput(const std::vector<size_t>& rowIndices) const {
    RowGroupBuilder builder(schema_);
    for (size_t rowIdx : rowIndices) {
        for (size_t colIdx = 0; colIdx < columns_.size(); ++colIdx) {
            std::visit([&](const auto& vec) {
                builder.Append<std::decay_t<decltype(vec[0])>>(colIdx, vec[rowIdx]);
            },
                       columns_[colIdx]);
        }
    }
    return builder.Finish();
}

}  // namespace Columnar::Exec
