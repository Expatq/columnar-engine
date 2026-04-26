#include "operator_runner.h"

namespace Columnar::Exec {

OperatorRunner::OperatorRunner(IOperator& root)
    : root_(root) {
}

OperatorRunner::~OperatorRunner() {
    Close();
}

void OperatorRunner::Open() {
    if (opened_) {
        return;
    }
    root_.Open();
    opened_ = true;
}

bool OperatorRunner::Next(ExecBatch& out) {
    return root_.Next(out);
}

void OperatorRunner::Close() noexcept {
    if (!opened_) {
        return;
    }
    root_.Close();
    opened_ = false;
}

}  // namespace Columnar::Exec
