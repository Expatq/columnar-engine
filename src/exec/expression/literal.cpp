#include "literal.h"

#include <exec/core/exec_batch.h>
#include <exec/interface/expression.h>

namespace Columnar::Exec {

ColumnSpan LiteralExpression::EvaluateColumn(const ExecBatch& /*input*/, EvalState& state) const {
    return std::visit([&](const auto& val) -> ColumnSpan {
        using T = std::decay_t<decltype(val)>;
        auto out = state.ResizeBuffer<T>(1);
        out[0] = val;
        return std::span<const T>{out.data(), 1};
    }, value_);
}

Types::AnyPhysicalType LiteralExpression::EvaluateScalar(const ExecBatch& /*input*/, RowId /*row*/) const {
    return value_;
}

}  // namespace Columnar::Exec
