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
    DateTruncExpression(std::unique_ptr<IExpression> input, DateTruncUnit unit);

    ExpressionKind Kind() const override;
    Types::LogicalType ResultType() const override;
    std::vector<std::string> RequiredColumns() const override;

    ColumnSpan EvaluateColumn(const ExecBatch& input, EvalState& state) const override;
    Types::AnyPhysicalType EvaluateScalar(const ExecBatch& input, RowId row) const override;

private:
    int64_t GetDivisor() const {
        switch (unit_) {
            case DateTruncUnit::Minute:
                return 60LL;
            case DateTruncUnit::Hour:
                return 3600LL;
            case DateTruncUnit::Day:
                return 86400LL;
            default:
                throw std::runtime_error("DateTrunc: unsupported unit");
        }
    }

private:
    std::unique_ptr<IExpression> input_;
    DateTruncUnit unit_;
    mutable EvalState inputState_;
};

}  // namespace Columnar::Exec
