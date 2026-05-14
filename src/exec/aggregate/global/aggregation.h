#pragma once

#include <exec/aggregate/lib/state.h>
#include <exec/aggregate/lib/spec.h>

#include <core/row_group.h>
#include <exec/core/exec_batch.h>

#include <exec/interface/operator.h>

#include <absl/container/inlined_vector.h>

#include <memory>

namespace Columnar::Exec {

class GlobalAggregation : public IOperator {
public:
    GlobalAggregation(std::unique_ptr<IOperator> child, std::vector<AggregateSpec> aggregates);

    void Open() override;
    bool Next(ExecBatch& out) override;
    void Close() noexcept override;

private:
    AggregateState MakeInitialState(const AggregateSpec& spec) const;
    void Consume(const ExecBatch& batch);
    RowGroup BuildResult() const;

    std::unique_ptr<IOperator> child_;
    absl::InlinedVector<AggregateSpec, 4> aggregates_;
    absl::InlinedVector<AggregateState, 4> states_;
    absl::InlinedVector<EvalState, 4> inputStates_;
    ExecBatch input_;
    bool produced_ = false;
};

}  // namespace Columnar::Exec
