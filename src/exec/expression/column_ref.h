#pragma once

#include <exec/interface/expression.h>

#include <string>
#include <vector>
#include "core/types.h"
#include "exec/core/exec_batch.h"
#include "exec/core/selection_vector.h"

namespace Columnar::Exec {

class ColumnRefExpression : public IExpression {
public:
    ColumnRefExpression(std::string column, Types::LogicalType type)
        : column_(std::move(column)),
          type_(std::move(type)) {
    }

    ExpressionKind Kind() const override {
        return ExpressionKind::ColumnRef;
    }

    Types::LogicalType ResultType() const override {
        return type_;
    }

    std::vector<std::string> RequiredColumns() const override {
        return {column_};
    }

    const std::string& Name() const {
        return column_;
    }

    ColumnSpan EvaluateColumn(const ExecBatch& input, EvalState& state) const override;

    Types::AnyPhysicalType EvaluateScalar(const ExecBatch& input, RowId row) const override;

private:
    std::string column_;
    Types::LogicalType type_;
};

}  // namespace Columnar::Exec
