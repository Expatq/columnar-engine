#include "case_when.h"

#include <stdexcept>
#include <absl/container/flat_hash_set.h>
#include "core/row_group.h"
#include "exec/core/selection_vector.h"

namespace Columnar::Exec {

namespace {

struct MergeSpans {
    size_t n;
    const uint8_t* condBit;
    bool hasSel;
    std::span<const RowId> selRows;
    EvalState& state;

    template <typename T>
    ColumnSpan operator()(std::span<const T> thenCol, std::span<const T> elseCol) const {
        auto out = state.ResizeBuffer<T>(n);
        for (size_t idx = 0; idx < n; ++idx) {
            const RowId physRow = hasSel ? selRows[idx] : static_cast<RowId>(idx);
            out[idx] = condBit[physRow] ? thenCol[idx] : elseCol[idx];
        }
        return std::span<const T>{out.data(), n};
    }

    template <typename T, typename E>
    ColumnSpan operator()(std::span<const T>, std::span<const E>) const {
        throw std::runtime_error("CaseWhen: THEN/ELSE physical type mismatch");
    }
};

}  // namespace

CaseWhenExpression::CaseWhenExpression(std::unique_ptr<IExpression> condition,
                                       std::unique_ptr<IExpression> then_expr,
                                       std::unique_ptr<IExpression> else_expr)
    : condition_(std::move(condition)),
      then_(std::move(then_expr)),
      else_(std::move(else_expr)) {
    if (!condition_ || !then_ || !else_)
        throw std::invalid_argument("CaseWhen: all branches required");
    if (condition_->ResultType() != Types::LogicalType::BOOL)
        throw std::invalid_argument("CaseWhen: condition must return BOOL");
    if (then_->ResultType() != else_->ResultType())
        throw std::invalid_argument("CaseWhen: THEN/ELSE type mismatch");
}

ExpressionKind CaseWhenExpression::Kind() const {
    return ExpressionKind::CaseWhen;
}
Types::LogicalType CaseWhenExpression::ResultType() const {
    return then_->ResultType();
}

std::vector<std::string> CaseWhenExpression::RequiredColumns() const {
    absl::flat_hash_set<std::string> cols;
    for (const auto& c : condition_->RequiredColumns()) cols.insert(c);
    for (const auto& c : then_->RequiredColumns()) cols.insert(c);
    for (const auto& c : else_->RequiredColumns()) cols.insert(c);
    return {cols.begin(), cols.end()};
}

ColumnSpan CaseWhenExpression::EvaluateColumn(const ExecBatch& input, EvalState& state) const {
    SelectionVector condSel;
    condition_->EvaluateSelection(input, condSel);

    uint8_t condBit[kBatchSize] = {};
    for (RowId row : condSel.Rows()) condBit[row] = 1;

    const ColumnSpan thenSpan = then_->EvaluateColumn(input, thenState_);
    const ColumnSpan elseSpan = else_->EvaluateColumn(input, elseState_);

    const size_t n = input.ActiveRowCount();
    return std::visit(MergeSpans{n, condBit, input.has_selection, input.selection.Rows(), state},
                      thenSpan, elseSpan);
}

Types::AnyPhysicalType CaseWhenExpression::EvaluateScalar(const ExecBatch& input, RowId row) const {
    SelectionVector singleRow;
    singleRow.Push(row);
    ExecBatch sub;
    sub.rowGroup = input.rowGroup;
    sub.rowCount = input.rowCount;
    sub.has_selection = true;
    sub.selection = std::move(singleRow);

    SelectionVector condResult;
    condition_->EvaluateSelection(sub, condResult);
    return condResult.Empty() ? else_->EvaluateScalar(input, row)
                              : then_->EvaluateScalar(input, row);
}

}  // namespace Columnar::Exec
