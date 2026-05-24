#pragma once

#include "sort_key.h"

#include <exec/core/exec_batch.h>
#include <exec/interface/operator.h>
#include <exec/result_format/row_group_builder.h>

#include <core/row_group.h>
#include <core/schema.h>
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
    struct BoundSortKey {
        size_t column = 0;
        Types::PhysicalType physical = Types::PhysicalType::INT64;
        bool descending = false;
    };

    struct CandidateView {
        const RowGroup* rowGroup = nullptr;
        RowId row = 0;
    };

    struct Candidate {
        std::shared_ptr<RowGroup> rowGroup;
        RowId row = 0;
    };

    void BindKeys(const RowGroup& rowGroup);
    bool IsLess(CandidateView lhs, CandidateView rhs) const;
    void ProcessBatch(const ExecBatch& batch);
    RowGroup BuildOutput() const;

    static int CompareByPhysicalType(const RowGroup& lhsGroup,
                                     RowId lhsRow,
                                     const RowGroup& rhsGroup,
                                     RowId rhsRow,
                                     const BoundSortKey& key);
    static CandidateView View(const Candidate& candidate);
    static void AppendCell(RowGroupBuilder& builder,
                           size_t outCol,
                           const RowGroup& rowGroup,
                           RowId row);

    std::unique_ptr<IOperator> child_;
    absl::InlinedVector<SortKey, 4> keys_;
    size_t limit_;
    size_t offset_;
    size_t maxKeep_;

    Schema schema_;
    absl::InlinedVector<BoundSortKey, 4> boundKeys_;
    std::vector<Candidate> heap_;
    bool produced_ = false;
    ExecBatch input_;
};

}  // namespace Columnar::Exec
