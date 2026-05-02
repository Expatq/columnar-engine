#pragma once

#include <core/types.h>
#include <exec/interface/expression.h>

#include <vector>

namespace Columnar::Exec {

class LiteralExpression : public IExpression {
public:
    LiteralExpression(Types::AnyPhysicalType value, Types::LogicalType type)
        : value_(std::move(value)),
          type_(type) {
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

private:
    Types::AnyPhysicalType value_;
    Types::LogicalType type_;
};

}  // namespace Columnar::Exec
