#pragma once

#include <exec/core/exec_batch.h>
#include <exec/core/selection_vector.h>
#include <exec/interface/expression.h>

#include <core/types.h>

#include <memory>
#include <regex>

namespace Columnar::Exec {

class RegexReplaceExpression : public IExpression {
public:
    RegexReplaceExpression(std::unique_ptr<IExpression> input, const std::string& pattern, std::string replacement);

    ExpressionKind Kind() const override;
    Types::LogicalType ResultType() const override;
    std::vector<std::string> RequiredColumns() const override;

    ColumnSpan EvaluateColumn(const ExecBatch& input, EvalState& state) const override;
    Types::AnyPhysicalType EvaluateScalar(const ExecBatch& input, RowId row) const override;

private:
    std::unique_ptr<IExpression> input_;
    std::regex pattern_;
    std::string replacement_;
    mutable EvalState inputState_;
};

}  // namespace Columnar::Exec
