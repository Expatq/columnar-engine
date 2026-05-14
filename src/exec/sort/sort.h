#pragma once

#include <exec/core/exec_batch.h>
#include <exec/interface/operator.h>
#include <exec/sort/sort_key.h>

#include <core/row_group.h>
#include <core/schema.h>
#include <core/types.h>

#include <absl/container/inlined_vector.h>

#include <memory>
#include <vector>

namespace Columnar::Exec {

class Sort : public IOperator {
public:
    Sort(std::unique_ptr<IOperator> child, std::vector<SortKey> keys);

    void Open() override;
    bool Next(ExecBatch& out) override;
    void Close() noexcept override;

private:
    bool IsLess(size_t lhsRowId, size_t rhsRowId) const;
    void AppendBatch(const ExecBatch& batch);

    RowGroup BuildOutput(const std::vector<size_t>& rowIndices) const;

    std::unique_ptr<IOperator> child_;
    absl::InlinedVector<SortKey, 4> keys_;

    Schema schema_;
    std::vector<Types::AnyColumnData> columns_;
    absl::InlinedVector<size_t, 4> keyColIndices_;
    size_t rowCount_ = 0;

    bool produced_ = false;
    ExecBatch input_;
};

}  // namespace Columnar::Exec
