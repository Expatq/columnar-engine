#include "extract.h"

#include <exec/core/exec_batch.h>
#include <exec/interface/expression.h>

#include <core/types.h>
#include <util/calendar.h>

#include <stdexcept>
#include <utility>

namespace Columnar::Exec {

namespace {

inline int16_t WrapField(int64_t bucket, int64_t remainder, int16_t period) {
    if (remainder < 0) {
        --bucket;
    }
    int64_t mod = bucket % period;
    if (mod < 0) {
        mod += period;
    }
    return static_cast<int16_t>(mod);
}

inline int16_t ExtractMinute(int64_t secondsSinceEpoch) {
    constexpr int64_t kDiv = Calendar::kSecondsPerMinute;
    return WrapField(secondsSinceEpoch / kDiv, secondsSinceEpoch % kDiv, 60);
}

inline int16_t ExtractHour(int64_t secondsSinceEpoch) {
    constexpr int64_t kDiv = Calendar::kSecondsPerHour;
    return WrapField(secondsSinceEpoch / kDiv, secondsSinceEpoch % kDiv, 24);
}

}  // namespace

ExtractExpression::ExtractExpression(std::unique_ptr<IExpression> input, ExtractField field)
    : inputExpr_(std::move(input)),
      field_(field) {
    if (!inputExpr_) {
        throw std::invalid_argument("Extract input cannot be null");
    }
    if (inputExpr_->ResultType() != Types::LogicalType::TIMESTAMP) {
        throw std::invalid_argument("Extract requires TIMESTAMP input");
    }
}

ExpressionKind ExtractExpression::Kind() const {
    return ExpressionKind::Extract;
}

Types::LogicalType ExtractExpression::ResultType() const {
    return Types::LogicalType::INT16;
}

std::vector<std::string> ExtractExpression::RequiredColumns() const {
    return inputExpr_->RequiredColumns();
}

ColumnSpan ExtractExpression::EvaluateColumn(const ExecBatch& input, EvalState& state) const {
    const ColumnSpan srcSpan = inputExpr_->EvaluateColumn(input, inputState_);
    const auto& ts = std::get<std::span<const int64_t>>(srcSpan);
    auto out = state.ResizeBuffer<int16_t>(ts.size());

    switch (field_) {
        case ExtractField::Minute:
            for (size_t i = 0; i < ts.size(); ++i)
                out[i] = ExtractMinute(ts[i]);
            break;
        case ExtractField::Hour:
            for (size_t i = 0; i < ts.size(); ++i)
                out[i] = ExtractHour(ts[i]);
            break;
    }
    return std::span<const int16_t>{out.data(), out.size()};
}

Types::AnyPhysicalType ExtractExpression::EvaluateScalar(const ExecBatch& input, RowId row) const {
    const int64_t ts = std::get<int64_t>(inputExpr_->EvaluateScalar(input, row));
    switch (field_) {
        case ExtractField::Minute:
            return ExtractMinute(ts);
        case ExtractField::Hour:
            return ExtractHour(ts);
    }
    std::unreachable();
}

}  // namespace Columnar::Exec
