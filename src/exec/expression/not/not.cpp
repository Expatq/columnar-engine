#include "not.h"
#include <cstdint>
#include "exec/core/selection_vector.h"

namespace Columnar::Exec {

NotExpression::NotExpression(std::unique_ptr<IExpression> operand)
    : operand_(std::move(operand)) {
    if (!operand_) {
        throw std::invalid_argument("operand cannot be null");
    }
    if (operand_->ResultType() != Types::LogicalType::BOOL) {
        throw std::invalid_argument("operand must return BOOL");
    }
}

ExpressionKind NotExpression::Kind() const {
    return ExpressionKind::Not;
}

Types::LogicalType NotExpression::ResultType() const {
    return Types::LogicalType::BOOL;
}

std::vector<std::string> NotExpression::RequiredColumns() const {
    return operand_->RequiredColumns();
}

void NotExpression::EvaluateSelection(const ExecBatch& input, SelectionVector& out) const {
    SelectionVector inner;
    operand_->EvaluateSelection(input, inner);

    std::vector<uint8_t> matched(input.rowCount, 0);
    for (RowId r : inner.Rows()) {
        matched[r] = 1;
    }

    out.Clear();
    if (input.has_selection) {
        for (RowId row : input.selection.Rows()) {
            if (!matched[row]) {
                out.Push(row);
            }
        }
    } else {
        for (RowId row = 0; row < input.rowCount; ++row) {
            if (!matched[row]) {
                out.Push(row);
            }
        }
    }
}

}  // namespace Columnar::Exec
