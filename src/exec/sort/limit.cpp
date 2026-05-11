#include "limit.h"

#include <exec/core/exec_batch.h>
#include <exec/result_format/row_group_builder.h>

#include <stdexcept>

namespace Columnar::Exec {

Limit::Limit(std::unique_ptr<IOperator> child, size_t limit, size_t offset)
    : child_(std::move(child)),
      limit_(limit),
      offset_(offset) {
    if (!child_)
        throw std::invalid_argument("Limit: child is null");
}

void Limit::Open() {
    child_->Open();
    produced_ = false;
}

bool Limit::Next(ExecBatch& out) {
    if (produced_) {
        return false;
    }

    size_t skipCnt = offset_;
    size_t takeCnt = limit_;
    bool schemaSet = false;
    RowGroupBuilder* builder = nullptr;
    std::optional<RowGroupBuilder> builderStorage;

    while (takeCnt > 0 && child_->Next(input_)) {
        if (!input_.rowGroup || input_.Empty()) {
            continue;
        }

        const RowGroup& rowGroup = *input_.rowGroup;
        if (!schemaSet) {
            schema_ = rowGroup.GetSchema();
            builderStorage.emplace(schema_);
            builder = &*builderStorage;
            schemaSet = true;
        }

        auto forActive = [&](auto&& func) {
            if (input_.has_selection) {
                for (RowId rowId : input_.selection.Rows()) {
                    func(rowId);
                }
            } else {
                for (RowId r = 0; r < static_cast<RowId>(input_.rowCount); ++r) {
                    func(r);
                }
            }
        };

        forActive([&](RowId rowId) {
            if (takeCnt == 0)
                return;
            if (skipCnt > 0) {
                --skipCnt;
                return;
            }
            for (size_t colIdx = 0; colIdx < rowGroup.GetColumnCount(); ++colIdx) {
                std::visit([&](const auto& vec) {
                    builder->Append<std::decay_t<decltype(vec[0])>>(colIdx, vec[rowId]);
                },rowGroup.GetColumn(colIdx).GetData());
            }
            --takeCnt;
        });
    }

    out.Reset();
    if (builderStorage.has_value()) {
        out.rowGroup = std::make_shared<RowGroup>(builderStorage->Finish());
        out.rowCount = out.rowGroup->GetRowCount();
    }
    produced_ = true;
    return out.rowCount > 0;
}

void Limit::Close() noexcept {
    child_->Close();
    produced_ = false;
}

}  // namespace Columnar::Exec
