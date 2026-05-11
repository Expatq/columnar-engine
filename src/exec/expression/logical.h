#pragma once

#include <exec/core/exec_batch.h>
#include <exec/core/selection_vector.h>
#include <exec/interface/expression.h>

#include <core/types.h>

#include <memory>

namespace Columnar::Exec {

/*
N-ary AND/OR: evaluates all operands left-to-right, returns combined boolean result.                                                             
AND short-circuits on first empty selection; OR accumulates union of matching rows. 
*/
class LogicalExpression : public IExpression {
public:
    LogicalExpression(LogicalOp op, std::vector<std::unique_ptr<IExpression>> operands);

    ExpressionKind Kind() const override;
    Types::LogicalType ResultType() const override;
    std::vector<std::string> RequiredColumns() const override;

    void EvaluateSelection(const ExecBatch& input, SelectionVector& out) const override;

private:
    void EvalAnd(const ExecBatch& input, SelectionVector& out) const;
    void EvalOr(const ExecBatch& input, SelectionVector& out) const;

    LogicalOp op_;
    std::vector<std::unique_ptr<IExpression>> operands_;
};

}  // namespace Columnar::Exec
