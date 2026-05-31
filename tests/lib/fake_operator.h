#pragma once

#include <exec/core/exec_batch.h>
#include <exec/interface/operator.h>

#include <cstddef>
#include <vector>

namespace Columnar::Test {

class FakeOperator : public Exec::IOperator {
public:
    explicit FakeOperator(std::vector<Exec::ExecBatch> batches);

    void Open() override;
    bool Next(Exec::ExecBatch& out) override;
    void Close() noexcept override;

private:
    std::vector<Exec::ExecBatch> batches_;
    size_t cursor_ = 0;
};

}  // namespace Columnar::Test
