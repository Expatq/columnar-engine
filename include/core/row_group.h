#pragma once

#include <core/batch.h>

#include <cstdint>

namespace Columnar {

struct RowGroupMeta {
    uint64_t offset = 0;  // offset in file (bytes)
    uint64_t size = 0;    // data size (bytes)
    uint32_t rowCount = 0;

    static constexpr size_t kSerializedSize = 20;
};

class RowGroup {
public:
    // ctors
    RowGroup() = default;

    explicit RowGroup(Batch batch);

    RowGroup(Batch batch, RowGroupMeta meta);

    // data access
    const Batch& GetBatch() const;
    Batch& GetMutableBatch();
    Batch&& MoveBatch();

    // meta
    const RowGroupMeta& GetMeta() const;
    RowGroupMeta& GetMutableMeta();
    void SetMeta(RowGroupMeta meta);

private:
    Batch batch_;
    RowGroupMeta meta_;
};

}  // namespace Columnar
