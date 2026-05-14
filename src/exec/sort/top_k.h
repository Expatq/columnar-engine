#pragma once

#include "sort_key.h"

#include <exec/core/exec_batch.h>

#include <exec/interface/expression.h>
#include <exec/interface/operator.h>

#include <core/row_group.h>
#include <core/types.h>

#include <absl/container/inlined_vector.h>

#include <memory>
#include <vector>

namespace Columnar::Exec {

class TopK : public IOperator {
public:
    TopK(std::unique_ptr<IOperator> child,
         std::vector<SortKey> keys,
         size_t limit,
         size_t offset = 0);

    void Open() override;
    bool Next(ExecBatch& out) override;
    void Close() noexcept override;

private:
    using Row = absl::InlinedVector<Types::AnyPhysicalType, 8>;

    bool IsLess(const Row& first, const Row& second) const;
    void ProcessBatch(const ExecBatch& batch);
    RowGroup BuildOutput() const;

    std::unique_ptr<IOperator> child_;
    absl::InlinedVector<SortKey, 4> keys_;
    size_t limit_;
    size_t offset_;

    Schema schema_;
    absl::InlinedVector<size_t, 4> keyColIndices_;
    std::vector<Row> heap_;
    bool produced_ = false;
    ExecBatch input_;
};

}  // namespace Columnar::Exec
