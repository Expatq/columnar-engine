#pragma once

#include <exec/interface/expression.h>

#include <memory>

namespace Columnar::Exec {

struct SortKey {
    std::unique_ptr<IExpression> expr;
    bool descending = false;
};

}  // namespace Columnar::Exec
