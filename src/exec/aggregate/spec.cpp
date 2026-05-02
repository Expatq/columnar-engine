#include "spec.h"

#include <stdexcept>
#include <utility>

namespace Columnar::Exec {
namespace {

std::unique_ptr<IExpression> RequireInput(std::unique_ptr<IExpression> input) {
    if (!input) {
        throw std::invalid_argument("aggregate input cannot be null");
    }
    return input;
}

}  // namespace

AggregateSpec CountStar(std::string outputName) {
    return AggregateSpec(
        AggregateKind::CountStar,
        nullptr,
        std::move(outputName),
        Types::LogicalType::INT64);
}

AggregateSpec CountColumn(std::unique_ptr<IExpression> input,
                          std::string outputName) {
    return AggregateSpec(
        AggregateKind::CountColumn,
        RequireInput(std::move(input)),
        std::move(outputName),
        Types::LogicalType::INT64);
}

AggregateSpec Sum(std::unique_ptr<IExpression> input,
                  std::string outputName,
                  Types::LogicalType outputType) {
    return AggregateSpec(
        AggregateKind::Sum,
        RequireInput(std::move(input)),
        std::move(outputName),
        outputType);
}

AggregateSpec Avg(std::unique_ptr<IExpression> input,
                  std::string outputName,
                  Types::LogicalType outputType) {
    return AggregateSpec(
        AggregateKind::Avg,
        RequireInput(std::move(input)),
        std::move(outputName),
        outputType);
}

AggregateSpec Min(std::unique_ptr<IExpression> input,
                  std::string outputName) {
    auto expr = RequireInput(std::move(input));
    const Types::LogicalType outputType = expr->ResultType();
    return AggregateSpec(
        AggregateKind::Min,
        std::move(expr),
        std::move(outputName),
        outputType);
}

AggregateSpec Max(std::unique_ptr<IExpression> input,
                  std::string outputName) {
    auto expr = RequireInput(std::move(input));
    const Types::LogicalType outputType = expr->ResultType();
    return AggregateSpec(
        AggregateKind::Max,
        std::move(expr),
        std::move(outputName),
        outputType);
}

}  // namespace Columnar::Exec
