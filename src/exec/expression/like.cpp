#include "like.h"

#include <exec/core/exec_batch.h>
#include <exec/core/selection_vector.h>
#include <exec/interface/expression.h>

#include <core/types.h>

namespace Columnar::Exec {

LikeExpression::LikeExpression(std::unique_ptr<IExpression> input, std::string sqlPattern)
    : input_(std::move(input)),
      pattern_(std::move(sqlPattern)) {
    if (!input_) {
        throw std::invalid_argument("LIKE input cannot be null");
    }
    if (input_->ResultType() != Types::LogicalType::STRING) {
        throw std::invalid_argument("LIKE requires STRING input");
    }
}

ExpressionKind LikeExpression::Kind() const {
    return ExpressionKind::Like;
}

Types::LogicalType LikeExpression::ResultType() const {
    return Types::LogicalType::BOOL;
}

std::vector<std::string> LikeExpression::RequiredColumns() const {
    return input_->RequiredColumns();
}

void LikeExpression::EvaluateSelection(const ExecBatch& input, SelectionVector& out) const {
    out.Clear();
    const ColumnSpan col = input_->EvaluateColumn(input, inputState_);
    const auto& strings = std::get<std::span<const std::string>>(col);

    if (!input.has_selection) {
        for (RowId row = 0; row < strings.size(); ++row) {
            if (pattern_.Match(strings[row])) {
                out.Push(row);
            }
        }
    } else {
        const auto& rows = input.selection.Rows();
        for (size_t idx = 0; idx < strings.size(); ++idx) {
            if (pattern_.Match(strings[idx])) {
                out.Push(rows[idx]);
            }
        }
    }
}

}  // namespace Columnar::Exec
