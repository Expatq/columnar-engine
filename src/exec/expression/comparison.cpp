#include "comparison.h"
#include "column_ref.h"
#include "literal.h"

#include <exec/core/exec_batch.h>
#include <exec/core/selection_vector.h>
#include <exec/interface/expression.h>

#include <core/types.h>

#include <util/assert.h>

#include <concepts>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace Columnar::Exec {

namespace {

template <typename Fn>
concept RowConsumer = requires(Fn fn, RowId row) {
    { fn(row) } -> std::same_as<void>;
};

template <RowConsumer Func>
void ForEachActiveRow(const ExecBatch& input, Func&& func) {
    if (input.has_selection) {
        for (RowId row : input.selection.Rows()) {
            func(row);
        }
        return;
    }

    for (RowId row = 0; row < input.rowCount; ++row) {
        func(row);
    }
}

template <typename T>
bool CompareValues(const T& left, CompareOp op, const T& right) {
    switch (op) {
        case CompareOp::Eq:
            return left == right;
        case CompareOp::NotEq:
            return left != right;
        case CompareOp::Lt:
            return left < right;
        case CompareOp::Lte:
            return left <= right;
        case CompareOp::Gt:
            return left > right;
        case CompareOp::Gte:
            return left >= right;
    }
    std::unreachable();
}

const Column& ResolveColumn(const ExecBatch& input, const std::string& name) {
    COLUMNAR_ASSERT(input.rowGroup.has_value(), "expression requires RowGroup");
    const Column* column = input.rowGroup->FindColumn(name);
    COLUMNAR_ASSERT(column != nullptr, "Unknown column: " + name);
    return *column;
}

template <typename T>
void EvalColumnLiteralTyped(const ExecBatch& input,
                            const Column& column,
                            const LiteralExpression& literal,
                            CompareOp op,
                            SelectionVector& output) {
    const auto& values = column.GetTypedData<T>();
    const T& constant = std::get<T>(literal.Value());

    ForEachActiveRow(input, [&](RowId row) {
        if (CompareValues(values[row], op, constant)) {
            output.Push(row);
        }
    });
}

template <typename T>
void EvalColumnColumnTyped(const ExecBatch& input,
                           const Column& leftColumn,
                           const Column& rightColumn,
                           CompareOp op,
                           SelectionVector& output) {
    const auto& left = leftColumn.GetTypedData<T>();
    const auto& right = rightColumn.GetTypedData<T>();

    ForEachActiveRow(input, [&](RowId row) {
        if (CompareValues(left[row], op, right[row])) {
            output.Push(row);
        }
    });
}

void ValidateComparableType(Types::LogicalType left, Types::LogicalType right) {
    if (Types::ToPhysical(left) != Types::ToPhysical(right)) {
        throw std::invalid_argument("comparison physical type mismatch");
    }
}

}  // namespace

ComparisonExpression::ComparisonExpression(std::unique_ptr<IExpression> left, CompareOp op, std::unique_ptr<IExpression> right)
    : left_(std::move(left)),
      op_(op),
      right_(std::move(right)) {
    if (!left_ || !right_) {
        throw std::invalid_argument("comparison operands cannot be null");
    }
    ValidateComparableType(left_->ResultType(), right_->ResultType());
}

ExpressionKind ComparisonExpression::Kind() const {
    return ExpressionKind::Comparison;
}

Types::LogicalType ComparisonExpression::ResultType() const {
    return Types::LogicalType::BOOL;
}

std::vector<std::string> ComparisonExpression::RequiredColumns() const {
    std::vector<std::string> result = left_->RequiredColumns();
    std::vector<std::string> rightColumns = right_->RequiredColumns();
    result.insert(result.end(), std::make_move_iterator(rightColumns.begin()), std::make_move_iterator(rightColumns.end()));
    return result;
}

void ComparisonExpression::EvaluateSelection(const ExecBatch& input, SelectionVector& output) const {
    output.Clear();

    if (left_->Kind() == ExpressionKind::ColumnRef &&
        right_->Kind() == ExpressionKind::Literal) {
        EvaluateColumnLiteral(
            input,
            static_cast<const ColumnRefExpression&>(*left_),
            static_cast<const LiteralExpression&>(*right_),
            output);
        return;
    }

    if (left_->Kind() == ExpressionKind::ColumnRef &&
        right_->Kind() == ExpressionKind::ColumnRef) {
        EvaluateColumnColumn(
            input,
            static_cast<const ColumnRefExpression&>(*left_),
            static_cast<const ColumnRefExpression&>(*right_),
            output);
        return;
    }

    throw std::runtime_error("comparison shape is not supported");
}

void ComparisonExpression::EvaluateColumnLiteral(
    const ExecBatch& input,
    const ColumnRefExpression& column,
    const LiteralExpression& literal,
    SelectionVector& output) const {
    const Column& data = ResolveColumn(input, column.Name());

    switch (Types::ToPhysical(column.ResultType())) {
        case Types::PhysicalType::INT16:
            EvalColumnLiteralTyped<int16_t>(input, data, literal, op_, output);
            return;
        case Types::PhysicalType::INT32:
            EvalColumnLiteralTyped<int32_t>(input, data, literal, op_, output);
            return;
        case Types::PhysicalType::INT64:
            EvalColumnLiteralTyped<int64_t>(input, data, literal, op_, output);
            return;
        case Types::PhysicalType::BOOL:
            EvalColumnLiteralTyped<uint8_t>(input, data, literal, op_, output);
            return;
        case Types::PhysicalType::STRING:
            EvalColumnLiteralTyped<std::string>(input, data, literal, op_, output);
            return;
    }
}

void ComparisonExpression::EvaluateColumnColumn(
    const ExecBatch& input,
    const ColumnRefExpression& left,
    const ColumnRefExpression& right,
    SelectionVector& output) const {
    const Column& leftData = ResolveColumn(input, left.Name());
    const Column& rightData = ResolveColumn(input, right.Name());

    switch (Types::ToPhysical(left.ResultType())) {
        case Types::PhysicalType::INT16:
            EvalColumnColumnTyped<int16_t>(
                input, leftData, rightData, op_, output);
            return;
        case Types::PhysicalType::INT32:
            EvalColumnColumnTyped<int32_t>(
                input, leftData, rightData, op_, output);
            return;
        case Types::PhysicalType::INT64:
            EvalColumnColumnTyped<int64_t>(
                input, leftData, rightData, op_, output);
            return;
        case Types::PhysicalType::BOOL:
            EvalColumnColumnTyped<uint8_t>(
                input, leftData, rightData, op_, output);
            return;
        case Types::PhysicalType::STRING:
            EvalColumnColumnTyped<std::string>(
                input, leftData, rightData, op_, output);
            return;
    }
}

}  // namespace Columnar::Exec
