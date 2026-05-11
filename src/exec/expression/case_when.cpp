#include "case_when.h"

#include <stdexcept>
#include <unordered_set>
#include "core/row_group.h"
#include "exec/core/selection_vector.h"

namespace Columnar::Exec {

CaseWhenExpression::CaseWhenExpression(std::unique_ptr<IExpression> condition, std::unique_ptr<IExpression> then_expr, std::unique_ptr<IExpression> else_expr)
    : condition_(std::move(condition)),
      then_(std::move(then_expr)),
      else_(std::move(else_expr)) {
    if (!condition_ || !then_ || !else_) {
        throw std::invalid_argument("CaseWhen: all branches required");
    }
    if (condition_->ResultType() != Types::LogicalType::BOOL) {
        throw std::invalid_argument("CaseWhen: condition must return BOOL");
    }
    if (then_->ResultType() != else_->ResultType()) {
        throw std::invalid_argument("CaseWhen: THEN/ELSE type mismatch");
    }
}

ExpressionKind CaseWhenExpression::Kind() const {
    return ExpressionKind::CaseWhen;
}

Types::LogicalType CaseWhenExpression::ResultType() const {
    return then_->ResultType();
}

std::vector<std::string> CaseWhenExpression::RequiredColumns() const {
    std::unordered_set<std::string> cols;
    for (const auto& c : condition_->RequiredColumns()) {
        cols.insert(c);
    }
    for (const auto& c : then_->RequiredColumns()) {
        cols.insert(c);
    }
    for (const auto& c : else_->RequiredColumns()) {
        cols.insert(c);
    }
    return {cols.begin(), cols.end()};
}

ColumnSpan CaseWhenExpression::EvaluateColumn(const ExecBatch& input, EvalState& state) const {
    SelectionVector condSel;
    condition_->EvaluateSelection(input, condSel);

    uint8_t condBit[kBatchSize] = {};
    for (RowId r : condSel.Rows()) {
        condBit[r] = 1;
    }

    const ColumnSpan thenSpan = then_->EvaluateColumn(input, thenState_);
    const ColumnSpan elseSpan = else_->EvaluateColumn(input, elseState_);

    const size_t n = input.ActiveRowCount();
    return std::visit([&]<typename T, typename E>(std::span<const T> thenCol, std::span<const E> elseCol) -> ColumnSpan {
        if constexpr (std::is_same_v<T, E>) {
            auto out = state.ResizeBuffer<T>(n);
            const bool hasSelection = input.has_selection;
            const auto& selRows = input.selection.Rows();
            
            for (size_t idx = 0; idx < n; ++idx) {
                const RowId physRow = hasSelection ? selRows[idx] : static_cast<RowId>(idx);
                out[idx] = condBit[physRow] ? thenCol[idx] : elseCol[idx];
            }
            return std::span<const T>{out.data(), n};
        } else {
            throw std::runtime_error("THEN/ELSE span type mismatch");
        }
    }, thenSpan, elseSpan);
}

Types::AnyPhysicalType CaseWhenExpression::EvaluateScalar(const ExecBatch& input, RowId row) const {
    SelectionVector singleRow;
    singleRow.Push(row);

    ExecBatch singleBatch;
    singleBatch.rowGroup = input.rowGroup;
    singleBatch.rowCount = input.rowCount;
    singleBatch.has_selection = true;
    singleBatch.selection = std::move(singleRow);

    SelectionVector condResult;
    condition_->EvaluateSelection(singleBatch, condResult);
    return condResult.Empty() ? else_->EvaluateScalar(input, row) : then_->EvaluateScalar(input, row);
}

}  // namespace Columnar::Exec
