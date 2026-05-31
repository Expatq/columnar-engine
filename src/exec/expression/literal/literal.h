#pragma once

#include <exec/core/exec_batch.h>
#include <exec/interface/expression.h>

#include <core/types.h>

#include <stdexcept>
#include <type_traits>
#include <vector>

namespace Columnar::Exec {

class LiteralExpression : public IExpression {
public:
    LiteralExpression(Types::AnyPhysicalType value, Types::LogicalType type)
        : value_(std::move(value)),
          type_(type) {
        if (value_.index() != Types::GetPhysVariantIndex(Types::ToPhysical(type_))) {
            throw std::invalid_argument(
                "Literal value type does not match logical type");
        }
    }

    ExpressionKind Kind() const override {
        return ExpressionKind::Literal;
    }

    Types::LogicalType ResultType() const override {
        return type_;
    }

    std::vector<std::string> RequiredColumns() const override {
        return {};
    }

    const Types::AnyPhysicalType& Value() const {
        return value_;
    }

    ColumnSpan EvaluateColumn(const ExecBatch& input, EvalState& state) const override {
        const size_t n = input.ActiveRowCount();
        return std::visit([&](const auto& val) -> ColumnSpan {
            using T = std::decay_t<decltype(val)>;
            auto buffer = state.ResizeBuffer<T>(n);
            std::fill(buffer.begin(), buffer.end(), val);
            return std::span<const T>{buffer.data(), n};
        },
                          value_);
    }

    Types::AnyPhysicalType EvaluateScalar(const ExecBatch& /*input*/, RowId /*row*/) const override {
        return value_;
    }

private:
    Types::AnyPhysicalType value_;
    Types::LogicalType type_;
};

}  // namespace Columnar::Exec
