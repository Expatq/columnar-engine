#include <tests/lib/fake_operator.h>

#include <utility>

namespace Columnar::Test {

FakeOperator::FakeOperator(std::vector<Exec::ExecBatch> batches)
    : batches_(std::move(batches)) {
}

void FakeOperator::Open() {
    cursor_ = 0;
}

bool FakeOperator::Next(Exec::ExecBatch& out) {
    if (cursor_ >= batches_.size()) {
        return false;
    }
    out = batches_[cursor_++];
    return true;
}

void FakeOperator::Close() noexcept {
}

}  // namespace Columnar::Test
