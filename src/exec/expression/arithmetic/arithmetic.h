#pragma once

#include <exec/core/exec_batch.h>
#include <exec/expression/column_ref/column_ref.h>
#include <exec/expression/literal/literal.h>
#include <exec/interface/expression.h>

#include <core/types.h>
#include <stdint.h>

#include <memory>
#include "exec/core/selection_vector.h"

namespace Columnar::Exec {

/*
As far as I know, modern columnar databases don't support arithmetic 
operations except with int64 and double (currently i dont support double)
*/
class ArithmeticExpression : public IExpression {
public:
    ArithmeticExpression(std::unique_ptr<IExpression> left, ArithmOp op, std::unique_ptr<IExpression> right);

    ExpressionKind Kind() const override;
    Types::LogicalType ResultType() const override;
    std::vector<std::string> RequiredColumns() const override;

    ColumnSpan EvaluateColumn(const ExecBatch& input, EvalState& state) const override;
    Types::AnyPhysicalType EvaluateScalar(const ExecBatch& input, RowId row) const override;

private:
    int64_t Apply(int64_t left, int64_t right) const;
    static int64_t ToInt64(const Types::AnyPhysicalType& value);

private:
    std::unique_ptr<IExpression> left_;
    ArithmOp op_;
    std::unique_ptr<IExpression> right_;
    mutable EvalState leftState_, rightState_;  // reused across const EvaluateColumn calls; mutable = per-call cache, not logical state
};

}  // namespace Columnar::Exec
