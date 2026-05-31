#pragma once

#include <exec/core/exec_batch.h>
#include <exec/core/selection_vector.h>
#include <exec/interface/expression.h>

#include <core/types.h>

#include <memory>
#include <vector>

namespace Columnar::Exec {

class StringLenExpression : public IExpression {
public:
    explicit StringLenExpression(std::unique_ptr<IExpression> input);

    ExpressionKind Kind() const override;
    Types::LogicalType ResultType() const override;
    std::vector<std::string> RequiredColumns() const override;

    ColumnSpan EvaluateColumn(const ExecBatch& input, EvalState& state) const override;
    Types::AnyPhysicalType EvaluateScalar(const ExecBatch& input, RowId row) const override;

private:
    std::unique_ptr<IExpression> input_;
    mutable EvalState inputState_;
};

}  // namespace Columnar::Exec
