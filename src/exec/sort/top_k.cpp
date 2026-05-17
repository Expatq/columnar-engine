#include "top_k.h"

#include <exec/interface/operator.h>
#include <exec/result_format/row_group_builder.h>

#include <core/row_group.h>

#include <util/assert.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace Columnar::Exec {

namespace {

template <typename T>
int CompareTyped(const RowGroup& lhsGroup,
                 RowId lhsRow,
                 const RowGroup& rhsGroup,
                 RowId rhsRow,
                 size_t column) {
    const auto& lhs = lhsGroup.GetColumn(column).GetTypedData<T>()[lhsRow];
    const auto& rhs = rhsGroup.GetColumn(column).GetTypedData<T>()[rhsRow];
    if (lhs < rhs) {
        return -1;
    }
    if (rhs < lhs) {
        return 1;
    }
    return 0;
}

}  // namespace

TopK::TopK(std::unique_ptr<IOperator> child,
           std::vector<SortKey> keys,
           size_t limit,
           size_t offset)
    : child_(std::move(child)),
      keys_(std::make_move_iterator(keys.begin()), std::make_move_iterator(keys.end())),
      limit_(limit),
      offset_(offset) {
    if (!child_ || keys_.empty() || limit_ == 0) {
        throw std::invalid_argument("TopK: invalid arguments");
    }
    if (offset_ > std::numeric_limits<size_t>::max() - limit_) {
        throw std::invalid_argument("TopK: limit + offset overflow");
    }
    maxKeep_ = limit_ + offset_;
}

void TopK::Open() {
    child_->Open();
    heap_.clear();
    boundKeys_.clear();
    input_.Reset();
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
    boundKeys_.clear();
    input_.Reset();
    produced_ = false;
}

int TopK::CompareByPhysicalType(const RowGroup& lhsGroup,
                                RowId lhsRow,
                                const RowGroup& rhsGroup,
                                RowId rhsRow,
                                const BoundSortKey& key) {
    switch (key.physical) {
        case Types::PhysicalType::INT16:
            return CompareTyped<int16_t>(lhsGroup, lhsRow, rhsGroup, rhsRow, key.column);
        case Types::PhysicalType::INT32:
            return CompareTyped<int32_t>(lhsGroup, lhsRow, rhsGroup, rhsRow, key.column);
        case Types::PhysicalType::INT64:
            return CompareTyped<int64_t>(lhsGroup, lhsRow, rhsGroup, rhsRow, key.column);
        case Types::PhysicalType::INT128:
            return CompareTyped<Int128>(lhsGroup, lhsRow, rhsGroup, rhsRow, key.column);
        case Types::PhysicalType::BOOL:
            return CompareTyped<uint8_t>(lhsGroup, lhsRow, rhsGroup, rhsRow, key.column);
        case Types::PhysicalType::STRING:
            return CompareTyped<std::string>(lhsGroup, lhsRow, rhsGroup, rhsRow, key.column);
    }
    COLUMNAR_ASSERT(false, "unknown physical type in TopK comparator");
}

void TopK::BindKeys(const RowGroup& rowGroup) {
    if (!boundKeys_.empty()) {
        return;
    }

    schema_ = rowGroup.GetSchema();
    for (const auto& key : keys_) {
        const auto cols = key.expr->RequiredColumns();
        COLUMNAR_ASSERT(!cols.empty(), "sort key requires a column reference");

        const auto idx = schema_.FindColumn(cols[0]);
        COLUMNAR_ASSERT(idx.has_value(), "sort key column not found in schema");

        const auto& column = schema_.GetColumn(*idx);
        boundKeys_.push_back(BoundSortKey{
            .column = *idx,
            .physical = column.physical,
            .descending = key.descending,
        });
    }
}

bool TopK::IsLess(CandidateView lhs, CandidateView rhs) const {
    COLUMNAR_ASSERT(lhs.rowGroup != nullptr, "TopK lhs row group is null");
    COLUMNAR_ASSERT(rhs.rowGroup != nullptr, "TopK rhs row group is null");

    for (const auto& key : boundKeys_) {
        const int cmp = CompareByPhysicalType(*lhs.rowGroup, lhs.row, *rhs.rowGroup, rhs.row, key);
        if (cmp == 0) {
            continue;
        }
        return key.descending ? cmp > 0 : cmp < 0;
    }
    return false;
}

void TopK::ProcessBatch(const ExecBatch& batch) {
    if (!batch.rowGroup || batch.Empty()) {
        return;
    }

    const RowGroup& rowGroup = *batch.rowGroup;
    BindKeys(rowGroup);

    auto heapCmp = [this](const Candidate& lhs, const Candidate& rhs) {
        return IsLess(View(lhs), View(rhs));
    };

    auto processRow = [&](RowId rowId) {
        const CandidateView incoming{.rowGroup = &rowGroup, .row = rowId};

        if (heap_.size() < maxKeep_) {
            heap_.push_back(Candidate{.rowGroup = batch.rowGroup, .row = rowId});
            std::push_heap(heap_.begin(), heap_.end(), heapCmp);
            return;
        }

        if (IsLess(incoming, View(heap_.front()))) {
            std::pop_heap(heap_.begin(), heap_.end(), heapCmp);
            heap_.back() = Candidate{.rowGroup = batch.rowGroup, .row = rowId};
            std::push_heap(heap_.begin(), heap_.end(), heapCmp);
        }
    };

    if (batch.has_selection) {
        for (RowId rowId : batch.selection.Rows()) {
            processRow(rowId);
        }
    } else {
        for (RowId rowId = 0; rowId < batch.rowCount; ++rowId) {
            processRow(rowId);
        }
    }
}

RowGroup TopK::BuildOutput() const {
    std::vector<const Candidate*> sorted;
    sorted.reserve(heap_.size());
    for (const auto& candidate : heap_) {
        sorted.push_back(&candidate);
    }

    std::sort(sorted.begin(), sorted.end(), [this](const Candidate* lhs, const Candidate* rhs) {
        return IsLess(View(*lhs), View(*rhs));
    });

    const size_t skip = std::min(offset_, sorted.size());
    const size_t available = sorted.size() - skip;
    const size_t take = std::min(limit_, available);

    RowGroupBuilder builder(schema_);
    for (size_t i = skip; i < skip + take; ++i) {
        const Candidate& candidate = *sorted[i];
        const RowGroup& rowGroup = *candidate.rowGroup;
        for (size_t col = 0; col < rowGroup.GetColumnCount(); ++col) {
            AppendCell(builder, col, rowGroup, candidate.row);
        }
    }
    return builder.Finish();
}

TopK::CandidateView TopK::View(const Candidate& candidate) {
    return CandidateView{
        .rowGroup = candidate.rowGroup.get(),
        .row = candidate.row,
    };
}

void TopK::AppendCell(RowGroupBuilder& builder,
                      size_t outCol,
                      const RowGroup& rowGroup,
                      RowId row) {
    const Column& column = rowGroup.GetColumn(outCol);
    switch (column.GetType()) {
        case Types::PhysicalType::INT16:
            builder.Append<int16_t>(outCol, column.GetTypedData<int16_t>()[row]);
            return;
        case Types::PhysicalType::INT32:
            builder.Append<int32_t>(outCol, column.GetTypedData<int32_t>()[row]);
            return;
        case Types::PhysicalType::INT64:
            builder.Append<int64_t>(outCol, column.GetTypedData<int64_t>()[row]);
            return;
        case Types::PhysicalType::INT128:
            builder.Append<Int128>(outCol, column.GetTypedData<Int128>()[row]);
            return;
        case Types::PhysicalType::BOOL:
            builder.Append<uint8_t>(outCol, column.GetTypedData<uint8_t>()[row]);
            return;
        case Types::PhysicalType::STRING:
            builder.Append<std::string>(outCol, column.GetTypedData<std::string>()[row]);
            return;
    }
    COLUMNAR_ASSERT(false, "unknown physical type in TopK output");
}

}  // namespace Columnar::Exec
