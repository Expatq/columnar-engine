#include "string_len.h"

namespace Columnar::Exec {

StringLenExpression::StringLenExpression(std::unique_ptr<IExpression> input)
    : input_(std::move(input)) {
    if (!input_) {
        throw std::invalid_argument("StringLen: input cannot be null");
    }
    if (input_->ResultType() != Types::LogicalType::STRING) {
        throw std::invalid_argument("StringLen: requires STRING input");
    }
}

ExpressionKind StringLenExpression::Kind() const {
    return ExpressionKind::StringLen;
}

Types::LogicalType StringLenExpression::ResultType() const {
    return Types::LogicalType::INT64;
}

std::vector<std::string> StringLenExpression::RequiredColumns() const {
    return input_->RequiredColumns();
}

ColumnSpan StringLenExpression::EvaluateColumn(const ExecBatch& input, EvalState& state) const {
    const ColumnSpan col = input_->EvaluateColumn(input, inputState_);
    const auto& strings = std::get<std::span<const std::string>>(col);

    auto out = state.ResizeBuffer<int64_t>(strings.size());
    for (size_t i = 0; i < strings.size(); ++i) {
        out[i] = static_cast<int64_t>(strings[i].size());
    }
    return std::span<const int64_t>{out.data(), out.size()};
}

Types::AnyPhysicalType StringLenExpression::EvaluateScalar(const ExecBatch& input, RowId row) const {
    return static_cast<int64_t>(std::get<std::string>(input_->EvaluateScalar(input, row)).size());
}

}  // namespace Columnar::Exec
