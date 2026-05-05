#pragma once

#include <exec/interface/expression.h>

#include <core/types.h>

#include <memory>

namespace Columnar::Exec {

enum class AggregateKind {
    CountStar,
    CountColumn,
    CountDistinct,
    Sum,
    Avg,
    Min,
    Max,
};

struct AggregateSpec {
    AggregateSpec(AggregateKind kind,
                  std::unique_ptr<IExpression> input,
                  std::string outputName,
                  Types::LogicalType outputType,
                  bool distinct = false)
        : kind(kind),
          input(std::move(input)),
          outputName(std::move(outputName)),
          outputType(outputType),
          distinct(distinct) {
    }

    AggregateSpec(const AggregateSpec&) = delete;
    AggregateSpec& operator=(const AggregateSpec&) = delete;

    AggregateSpec(AggregateSpec&&) noexcept = default;
    AggregateSpec& operator=(AggregateSpec&&) noexcept = default;

    bool HasInput() const {
        return input != nullptr;
    }

    Types::LogicalType InputType() const {
        if (!input) {
            throw std::logic_error("aggregate has no input expression");
        }
        return input->ResultType();
    }

    AggregateKind kind;
    std::unique_ptr<IExpression> input;  // nulptr if COUNT(*)
    std::string outputName;
    Types::LogicalType outputType;
    bool distinct = false;
};

AggregateSpec CountStar(std::string outputName);
AggregateSpec CountColumn(std::unique_ptr<IExpression> input, std::string outputName);
AggregateSpec CountDistinct(std::unique_ptr<IExpression> input, std::string outputName);
AggregateSpec Sum(std::unique_ptr<IExpression> input, std::string outputName, Types::LogicalType outputType);
AggregateSpec Avg(std::unique_ptr<IExpression> input, std::string outputName, Types::LogicalType outputType);
AggregateSpec Min(std::unique_ptr<IExpression> input, std::string outputName);
AggregateSpec Max(std::unique_ptr<IExpression> input, std::string outputName);

}  // namespace Columnar::Exec
