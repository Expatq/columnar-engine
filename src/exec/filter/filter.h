#pragma once

#include <exec/core/exec_batch.h>
#include <exec/interface/expression.h>
#include <exec/interface/operator.h>

#include <memory>

namespace Columnar::Exec {

class Filter : public IOperator {
public:
    Filter(std::unique_ptr<IOperator> child,
           std::unique_ptr<IExpression> condition);

    void Open() override;
    bool Next(ExecBatch& out) override;
    void Close() noexcept override;

private:
    std::unique_ptr<IOperator> child_;
    std::unique_ptr<IExpression> condition_;
    ExecBatch input_;
};

}  // namespace Columnar::Exec
