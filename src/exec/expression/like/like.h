#pragma once

#include "like_pattern.h"

#include <exec/core/exec_batch.h>
#include <exec/core/selection_vector.h>
#include <exec/interface/expression.h>

namespace Columnar::Exec {

class LikeExpression : public IExpression {
public:
    LikeExpression(std::unique_ptr<IExpression> input, std::string sqlPattern);

    ExpressionKind Kind() const override;
    Types::LogicalType ResultType() const override;
    std::vector<std::string> RequiredColumns() const override;

    void EvaluateSelection(const ExecBatch& input, SelectionVector& out) const override;

private:
    std::unique_ptr<IExpression> input_;
    CompiledPattern pattern_;
    mutable EvalState inputState_;
};

}  // namespace Columnar::Exec
