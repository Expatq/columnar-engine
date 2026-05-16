#include "top_k.h"

#include <exec/interface/operator.h>
#include <exec/core/selection_vector.h>
#include <exec/result_format/row_group_builder.h>

#include <core/row_group.h>

#include <util/assert.h>

#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace Columnar::Exec {

TopK::TopK(std::unique_ptr<IOperator> child, std::vector<SortKey> keys, size_t limit, size_t offset)
    : child_(std::move(child)),
      keys_(std::make_move_iterator(keys.begin()), std::make_move_iterator(keys.end())),
      limit_(limit),
      offset_(offset) {
    if (!child_ || keys_.empty() || limit_ == 0) {
        throw std::invalid_argument("TopK: invalid arguments");
    }
}

void TopK::Open() {
    child_->Open();
    heap_.clear();
    keyColIndices_.clear();
    produced_ = false;
}

bool TopK::Next(ExecBatch& out) {
    if (produced_) {
        return false;
    }

    while (child_->Next(input_)) {
        ProcessBatch(input_);
    }
    out.Reset();
    out.rowGroup = std::make_shared<RowGroup>(BuildOutput());
    out.rowCount = out.rowGroup->GetRowCount();
    produced_ = true;
    return out.rowCount > 0;
}

void TopK::Close() noexcept {
    child_->Close();
    heap_.clear();
    keyColIndices_.clear();
    produced_ = false;
}

bool TopK::IsLess(const Row& first, const Row& second) const {
    for (size_t i = 0; i < keys_.size(); ++i) {
        const auto& firstValue = first[keyColIndices_[i]];
        const auto& secondValue = second[keyColIndices_[i]];

        if (firstValue == secondValue) {
            continue;
        }
        const bool less = std::visit([&](const auto& firstV) {
            return firstV < std::get<std::decay_t<decltype(firstV)>>(secondValue);
        },
                                     firstValue);
        return keys_[i].descending ? !less : less;
    }
    return false;
}

void TopK::ProcessBatch(const ExecBatch& batch) {
    if (!batch.rowGroup) {
        return;
    }

    const RowGroup& rowGroup = *batch.rowGroup;
    if (keyColIndices_.empty()) {
        schema_ = rowGroup.GetSchema();
        for (const auto& key : keys_) {
            const auto cols = key.expr->RequiredColumns();
            COLUMNAR_ASSERT(!cols.empty(), "sort key requires a column reference");
            const auto idx = schema_.FindColumn(cols[0]);  // TODO: fix dirty hack with cols[0], because ORDER BY (A + B) wont work
            COLUMNAR_ASSERT(idx.has_value(), "sort key column not found in schema");
            keyColIndices_.push_back(*idx);
        }
    }

    if (batch.Empty()) {
        return;
    }

    const size_t maxKeep = limit_ + offset_;
    auto heapCmp = [this](const Row& lhs, const Row& rhs) { return IsLess(lhs, rhs); };
    auto forActiveRows = [&](auto&& func) {
        if (batch.has_selection) {
            for (RowId row : batch.selection.Rows()) {
                func(row);
            }
        } else {
            for (RowId row = 0; row < batch.rowCount; ++row) {
                func(row);
            }
        }
    };

    forActiveRows([&](RowId rowId) {
        Row row;
        row.reserve(rowGroup.GetColumnCount());
        for (size_t colIdx = 0; colIdx < rowGroup.GetColumnCount(); ++colIdx) {
            std::visit([&](const auto& vec) {
                row.push_back(vec[rowId]);
            },
                       rowGroup.GetColumn(colIdx).GetData());
        }

        if (heap_.size() < maxKeep) {
            heap_.push_back(std::move(row));
            std::push_heap(heap_.begin(), heap_.end(), heapCmp);
        } else if (IsLess(row, heap_.front())) {
            std::pop_heap(heap_.begin(), heap_.end(), heapCmp);
            heap_.back() = std::move(row);
            std::push_heap(heap_.begin(), heap_.end(), heapCmp);
        }
    });
}

RowGroup TopK::BuildOutput() const {
    std::vector<const Row*> sorted;
    sorted.reserve(heap_.size());

    for (const auto& row : heap_) {
        sorted.push_back(&row);
    };

    std::sort(sorted.begin(), sorted.end(),
              [this](const Row* lhs, const Row* rhs) { return IsLess(*lhs, *rhs); });

    const size_t skipCnt = std::min(offset_, sorted.size());
    const size_t takeCnt = std::min(limit_, sorted.size() > skipCnt ? sorted.size() - skipCnt : size_t{0});

    RowGroupBuilder builder(schema_);
    for (size_t i = skipCnt; i < skipCnt + takeCnt; ++i) {
        const Row& row = *sorted[i];
        for (size_t c = 0; c < row.size(); ++c) {
            std::visit([&](const auto& val) {
                builder.Append<std::decay_t<decltype(val)>>(c, val);
            },row[c]);
        }
    }
    return builder.Finish();
}

}  // namespace Columnar::Exec
