#include "date_trunc.h"

#include <exec/core/exec_batch.h>
#include <exec/interface/expression.h>

#include <core/types.h>
#include <util/calendar.h>

#include <utility>

namespace Columnar::Exec {

namespace {

inline int64_t FloorBucket(int64_t value, int64_t div) {
    int64_t bucket = value / div;
    if (value < 0 && value % div != 0) {
        --bucket;
    }
    return bucket * div;
}

}  // namespace

DateTruncExpression::DateTruncExpression(std::unique_ptr<IExpression> input, DateTruncUnit unit)
    : inputExpr_(std::move(input)),
      unit_(unit) {
    if (!inputExpr_) {
        throw std::invalid_argument("DateTrunc input cannot be null");
    }
    if (inputExpr_->ResultType() != Types::LogicalType::TIMESTAMP) {
        throw std::invalid_argument("DateTrunc requires TIMESTAMP input");
    }
}

ExpressionKind DateTruncExpression::Kind() const {
    return ExpressionKind::DateTrunc;
}

Types::LogicalType DateTruncExpression::ResultType() const {
    return Types::LogicalType::TIMESTAMP;
}

std::vector<std::string> DateTruncExpression::RequiredColumns() const {
    return inputExpr_->RequiredColumns();
}

ColumnSpan DateTruncExpression::EvaluateColumn(const ExecBatch& input, EvalState& state) const {
    const ColumnSpan srcSpan = inputExpr_->EvaluateColumn(input, inputState_);
    const auto& ts = std::get<std::span<const int64_t>>(srcSpan);
    auto out = state.ResizeBuffer<int64_t>(ts.size());

    const int64_t div = GetDivisor();
    for (size_t i = 0; i < ts.size(); ++i) {
        out[i] = FloorBucket(ts[i], div);
    }
    return std::span<const int64_t>{out.data(), out.size()};
}

Types::AnyPhysicalType DateTruncExpression::EvaluateScalar(const ExecBatch& input, RowId row) const {
    const int64_t ts = std::get<int64_t>(inputExpr_->EvaluateScalar(input, row));
    return FloorBucket(ts, GetDivisor());
}

int64_t DateTruncExpression::GetDivisor() const {
    switch (unit_) {
        case DateTruncUnit::Minute:
            return Calendar::kSecondsPerMinute;
        case DateTruncUnit::Hour:
            return Calendar::kSecondsPerHour;
        case DateTruncUnit::Day:
            return Calendar::kSecondsPerDay;
    }
    std::unreachable();
}

}  // namespace Columnar::Exec
