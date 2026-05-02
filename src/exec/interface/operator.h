#pragma once

#include <exec/core/exec_batch.h>

namespace Columnar::Exec {

class IOperator {
public:
    virtual ~IOperator() = default;

    virtual void Open() = 0;

    virtual bool Next(ExecBatch& out) = 0;

    virtual void Close() noexcept = 0;
};

}  // namespace Columnar::Exec
