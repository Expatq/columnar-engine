#pragma once

#include <exec/core/exec_batch.h>
#include <exec/core/selection_vector.h>
#include <exec/interface/expression.h>

#include <memory>

namespace Columnar::Exec {

class ComparisonExpression : public IExpression {
public:
    ComparisonExpression(std::unique_ptr<IExpression> left, CompareOp op, std::unique_ptr<IExpression> right);

    ExpressionKind Kind() const override;
    Types::LogicalType ResultType() const override;
    std::vector<std::string> RequiredColumns() const override;

    void EvaluateSelection(const ExecBatch& input, SelectionVector& output) const override;

private:
    void EvaluateColumnLiteral(const ExecBatch& input,
                               const class ColumnRefExpression& column,
                               const class LiteralExpression& literal,
                               SelectionVector& output) const;

    void EvaluateColumnColumn(const ExecBatch& input,
                              const class ColumnRefExpression& left,
                              const class ColumnRefExpression& right,
                              SelectionVector& output) const;

    std::unique_ptr<IExpression> left_;
    CompareOp op_;
    std::unique_ptr<IExpression> right_;
};

}  // namespace Columnar::Exec
