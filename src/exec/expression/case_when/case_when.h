#pragma once

#include <core/types.h>
#include <exec/core/exec_batch.h>
#include <exec/interface/expression.h>

#include <memory>
#include <vector>
#include "exec/core/selection_vector.h"

namespace Columnar::Exec {

class CaseWhenExpression : public IExpression {
public:
    CaseWhenExpression(std::unique_ptr<IExpression> condition, std::unique_ptr<IExpression> then_expr, std::unique_ptr<IExpression> else_expr);

    ExpressionKind Kind() const override;
    Types::LogicalType ResultType() const override;
    std::vector<std::string> RequiredColumns() const override;

    ColumnSpan EvaluateColumn(const ExecBatch& input, EvalState& state) const override;
    Types::AnyPhysicalType EvaluateScalar(const ExecBatch& input, RowId row) const override;

private:
    std::unique_ptr<IExpression> condition_;
    std::unique_ptr<IExpression> then_;
    std::unique_ptr<IExpression> else_;
    mutable EvalState thenState_, elseState_;
};

}  // namespace Columnar::Exec
