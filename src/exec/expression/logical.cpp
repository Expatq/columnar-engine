#include "logical.h"

#include <exec/core/required_columns.h>
#include <exec/core/exec_batch.h>
#include <exec/core/selection_vector.h>
#include <exec/interface/expression.h>

#include <core/row_group.h>
#include <core/types.h>

#include <cstdint>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <unordered_set>

namespace Columnar::Exec {

LogicalExpression::LogicalExpression(LogicalOp op, std::vector<std::unique_ptr<IExpression>> operands)
    : op_(op),
      operands_(std::move(operands)) {
    if (operands_.size() < 2) {
        throw std::invalid_argument("logical expression needs at least 2 operands");
    }
    for (const auto& expr : operands_) {
        if (expr->ResultType() != Types::LogicalType::BOOL) {
            throw std::invalid_argument("logical operand must return BOOL");
        }
    }
}

ExpressionKind LogicalExpression::Kind() const {
    return ExpressionKind::Logical;
}

Types::LogicalType LogicalExpression::ResultType() const {
    return Types::LogicalType::BOOL;
}

std::vector<std::string> LogicalExpression::RequiredColumns() const {
    std::unordered_set<std::string> reqCols;
    for (const auto& expr : operands_) {
        auto cols = expr->RequiredColumns();
        reqCols.insert(std::make_move_iterator(cols.begin()), std::make_move_iterator(cols.end()));
    }

    return {std::make_move_iterator(reqCols.begin()), std::make_move_iterator(reqCols.end())};
}

void LogicalExpression::EvaluateSelection(const ExecBatch& input, SelectionVector& out) const {
    switch (op_) {
        case LogicalOp::And: {
            EvalAnd(input, out);
        }
        case LogicalOp::Or: {
            EvalOr(input, out);
        }
    }
}

void LogicalExpression::EvalAnd(const ExecBatch& input, SelectionVector& out) const {
    operands_[0]->EvaluateSelection(input, out);
    SelectionVector tmp;
    ExecBatch sub;
    sub.rowGroup = input.rowGroup;
    sub.rowCount = input.rowCount;
    sub.has_selection = true;
    for (size_t i = 1; i < operands_.size(); ++i) {
        sub.selection = std::move(out);
        tmp.Clear();
        operands_[i]->EvaluateSelection(sub, tmp);
        out = std::move(tmp);
    }
}

void LogicalExpression::EvalOr(const ExecBatch& input, SelectionVector& out) const {
    uint8_t selected[kBatchSize] = {};

    SelectionVector remaining;
    if (input.has_selection) {
        remaining = input.selection;
    } else {
        remaining.MutableRows().reserve(input.rowCount);
        std::iota(remaining.MutableRows().begin(), remaining.MutableRows().end(), 0);
    }

    ExecBatch subBatch;
    subBatch.rowGroup = input.rowGroup;
    subBatch.rowCount = input.rowCount;
    subBatch.has_selection = true;

    SelectionVector tmp;
    SelectionVector nextRemaining;

    for (const auto& expr : operands_) {
        if (remaining.Empty()) {
            break;
        }

        subBatch.selection = std::move(remaining);
        tmp.Clear();
        expr->EvaluateSelection(subBatch, tmp);

        for (RowId row : tmp.Rows()) {
            selected[row] = 1;
        }

        nextRemaining.Clear();
        for (RowId row : subBatch.selection.Rows()) {
            if (!selected[row]) {
                nextRemaining.Push(row);
            }
        }
        remaining = std::move(nextRemaining);
    }

    out.Clear();
    if (input.has_selection) {
        for (RowId row : input.selection.Rows()) {
            if (selected[row]) {
                out.Push(row);
            }
        }
    } else {
        for (RowId row = 0; row < input.rowCount; ++row) {
            if (selected[row]) {
                out.Push(row);
            }
        }
    }
}

}  // namespace Columnar::Exec
