#include "filter.h"

#include <core/types.h>
#include <exec/core/exec_batch.h>
#include <exec/interface/expression.h>

#include <stdexcept>

namespace Columnar::Exec {

Filter::Filter(std::unique_ptr<IOperator> child, std::unique_ptr<IExpression> condition)
    : child_(std::move(child)),
      condition_(std::move(condition)) {
    if (!child_ || !condition_) {
        throw std::invalid_argument("Filter requires child and condition");
    }
    if (condition_->ResultType() != Types::LogicalType::BOOL) {
        throw std::invalid_argument("Filter condition must be bool");
    }
}

void Filter::Open() {
    child_->Open();
}

bool Filter::Next(ExecBatch& out) {
    while (child_->Next(input_)) {
        out.Reset();
        condition_->EvaluateSelection(input_, out.selection);
        out.has_selection = true;
        out.rowCount = input_.rowCount;
        out.rowGroup = std::move(input_.rowGroup);

        if (!out.Empty()) {
            return true;
        }
    }
    return false;
}

void Filter::Close() noexcept {
    child_->Close();
    input_.Reset();
}

}  // namespace Columnar::Exec
