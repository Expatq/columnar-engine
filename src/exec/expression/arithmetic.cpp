#include "arithmetic.h"
#include <stdint.h>

#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include "core/types.h"
#include "exec/core/exec_batch.h"
#include "exec/interface/expression.h"

namespace Columnar::Exec {

ArithmeticExpression::ArithmeticExpression(std::unique_ptr<IExpression> left, ArithmOp op, std::unique_ptr<IExpression> right)
    : left_(std::move(left)),
      op_(op),
      right_(std::move(right)) {
    if (!left_ || !right_) {
        throw std::invalid_argument("arithmetic operands cannot be null");
    }
}

ExpressionKind ArithmeticExpression::Kind() const {
    return ExpressionKind::Arithmetic;
}

Types::LogicalType ArithmeticExpression::ResultType() const {
    return Types::LogicalType::INT64;
}

std::vector<std::string> ArithmeticExpression::RequiredColumns() const {
    std::unordered_set<std::string> reqCols;
    auto colsL = left_->RequiredColumns();
    auto colsR = right_->RequiredColumns();

    reqCols.insert(std::make_move_iterator(colsL.begin()), std::make_move_iterator(colsL.end()));
    reqCols.insert(std::make_move_iterator(colsR.begin()), std::make_move_iterator(colsR.end()));

    return {reqCols.begin(), reqCols.end()};
}

ColumnSpan ArithmeticExpression::EvaluateColumn(const ExecBatch& input, EvalState& state) const {
    const ColumnSpan lspan = left_->EvaluateColumn(input, leftState_);
    const ColumnSpan rspan = right_->EvaluateColumn(input, rightState_);

    return std::visit([&]<typename LType, typename RType>(std::span<LType> leftCol, std::span<RType> rightCol) -> ColumnSpan {
        if constexpr (std::is_integral_v<LType> && std::is_integral_v<RType>) {
            const bool isScalarL = leftCol.size() == 1;
            const bool isScalarR = rightCol.size() == 1;
            const size_t resultSize = isScalarL ? rightCol.size() : leftCol.size();

            auto out = state.ResizeBuffer<int64_t>(resultSize);
            for (size_t i = 0; i < resultSize; ++i) {
                out[i] = Apply(static_cast<int64_t>(isScalarL ? leftCol[0] : leftCol[i]), static_cast<int64_t>(isScalarR ? rightCol[0] : rightCol[i]));
            }
            return std::span<int64_t>{out.data(), out.size()};
        } else {
            throw std::runtime_error("arithmetic on non-integer types");
        }
    }, lspan, rspan);
}

Types::AnyPhysicalType ArithmeticExpression::EvaluateScalar(const ExecBatch& input, RowId row) const {
    return Apply(ToInt64(left_->EvaluateScalar(input, row)), ToInt64(right_->EvaluateScalar(input, row)));
}

int64_t ArithmeticExpression::Apply(int64_t left, int64_t right) const {
    switch (op_) {
        case ArithmOp::Add:
            return left + right;
        case ArithmOp::Sub:
            return left - right;
        case ArithmOp::Mul:
            return left * right;
        case ArithmOp::Div:
            if (right == 0) {
                throw std::runtime_error("division by zero");
            }
            return left / right;
    }
    std::unreachable();
}

int64_t ArithmeticExpression::ToInt64(const Types::AnyPhysicalType& value) {
    return std::visit([](const auto& val) -> int64_t {
        if constexpr (std::is_integral_v<std::decay_t<decltype(val)>>) {
            return static_cast<int64_t>(val);
        }
        throw std::runtime_error("cannot convert to int64");
    }, value);
}

}  // namespace Columnar::Exec
