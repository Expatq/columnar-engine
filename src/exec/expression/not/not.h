#pragma once

#include <exec/core/exec_batch.h>
#include <exec/core/selection_vector.h>
#include <exec/interface/expression.h>

#include <core/row_group.h>
#include <core/types.h>

#include <memory>
#include <stdexcept>
#include <vector>

namespace Columnar::Exec {

class NotExpression : public IExpression {
public:
    explicit NotExpression(std::unique_ptr<IExpression> operand);

    ExpressionKind Kind() const override;
    Types::LogicalType ResultType() const override;
    std::vector<std::string> RequiredColumns() const override;

    void EvaluateSelection(const ExecBatch& input, SelectionVector& out) const override;

private:
    std::unique_ptr<IExpression> operand_;
};

}  // namespace Columnar::Exec
