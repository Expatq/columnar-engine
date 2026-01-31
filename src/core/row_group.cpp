#include <core/batch.h>
#include <core/row_group.h>

#include <cstdint>

namespace Columnar {

RowGroup::RowGroup(Batch batch)
    : batch_(std::move(batch)) {
    meta_.rowCount = static_cast<uint32_t>(batch.GetColumnCount());
}

RowGroup::RowGroup(Batch batch, RowGroupMeta meta)
    : batch_(std::move(batch)),
      meta_(meta) {}

const Batch& RowGroup::GetBatch() const {
    return batch_;
}

Batch& RowGroup::GetMutableBatch() {
    return batch_;
}

Batch&& RowGroup::MoveBatch() {
    return std::move(batch_);
}

const RowGroupMeta& RowGroup::GetMeta() const {
    return meta_;
}

RowGroupMeta& RowGroup::GetMutableMeta() {
    return meta_;
}

void RowGroup::SetMeta(RowGroupMeta meta) {
    meta_ = meta;
}

}  // namespace Columnar