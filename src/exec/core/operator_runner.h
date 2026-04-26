#pragma once

#include <exec/interface/operator.h>

namespace Columnar::Exec {

class OperatorRunner {
public:
    explicit OperatorRunner(IOperator& root);
    ~OperatorRunner();

    OperatorRunner(const OperatorRunner&) = delete;
    OperatorRunner& operator=(const OperatorRunner&) = delete;

    void Open();
    bool Next(ExecBatch& out);
    void Close() noexcept;

private:
    IOperator& root_;
    bool opened_ = false;
};

}  // namespace Columnar::Exec
