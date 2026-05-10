#pragma once

#include "sort_key.h"

#include <exec/core/exec_batch.h>

#include <exec/interface/expression.h>
#include <exec/interface/operator.h>

#include <core/row_group.h>
#include <core/types.h>

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
    using Row = std::vector<Types::AnyPhysicalType>;

    bool IsLess(const Row& first, const Row& second) const;
    void ProcessBatch(const ExecBatch& batch);
    RowGroup BuildOutput() const;

    std::unique_ptr<IOperator> child_;
    std::vector<SortKey> keys_;
    size_t limit_;
    size_t offset_;

    Schema schema_;
    std::vector<size_t> keyColIndices_;
    std::vector<Row> heap_;
    bool produced_ = false;
    ExecBatch input_;
};

}  // namespace Columnar::Exec
