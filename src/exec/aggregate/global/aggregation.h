#pragma once

#include <exec/aggregate/lib/state.h>
#include <exec/aggregate/lib/spec.h>

#include <core/row_group.h>
#include <exec/core/exec_batch.h>

#include <exec/interface/operator.h>

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
    std::vector<AggregateSpec> aggregates_;
    std::vector<AggregateState> states_;
    std::vector<EvalState> inputStates_;
    ExecBatch input_;
    bool produced_ = false;
};

}  // namespace Columnar::Exec
