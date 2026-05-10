#pragma once

#include <exec/core/exec_batch.h>
#include <exec/core/selection_vector.h>
#include <exec/interface/expression.h>

#include <core/types.h>
#include <stdint.h>

#include <memory>
#include <vector>

namespace Columnar::Exec {

class DateTruncExpression : public IExpression {
public:
    DateTruncExpression(std::unique_ptr<IExpression> inputExpr, DateTruncUnit unit);

    ExpressionKind Kind() const override;
    Types::LogicalType ResultType() const override;
    std::vector<std::string> RequiredColumns() const override;

    ColumnSpan EvaluateColumn(const ExecBatch& input, EvalState& state) const override;
    Types::AnyPhysicalType EvaluateScalar(const ExecBatch& input, RowId row) const override;

private:
    int64_t GetDivisor() const;

private:
    std::unique_ptr<IExpression> inputExpr_;
    DateTruncUnit unit_;
    mutable EvalState inputState_;
};

}  // namespace Columnar::Exec
