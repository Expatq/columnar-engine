#pragma once

#include <exec/core/exec_batch.h>
#include <exec/interface/operator.h>

#include <core/schema.h>

#include <memory>

namespace Columnar::Exec {

class Limit : public IOperator {
public:
    Limit(std::unique_ptr<IOperator> child, size_t limit, size_t offset = 0);

    void Open() override;
    bool Next(ExecBatch& out) override;
    void Close() noexcept override;

private:
    std::unique_ptr<IOperator> child_;
    size_t limit_;
    size_t offset_;

    Schema schema_;
    bool produced_ = false;
    ExecBatch input_;
};

}  // namespace Columnar::Exec
