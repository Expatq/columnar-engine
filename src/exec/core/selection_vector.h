#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace Columnar::Exec {

using RowId = uint32_t;

class SelectionVector {
public:
    void Push(RowId row) {
        rows_.push_back(row);
    }

    bool Empty() const {
        return rows_.empty();
    }

    void Clear() {
        rows_.clear();
    }

    size_t Size() const {
        return rows_.size();
    }

    std::span<const RowId> Rows() const {
        return rows_;
    }

    std::vector<RowId>& MutableRows() {
        return rows_;
    }

private:
    std::vector<RowId> rows_;
};

}  // namespace Columnar::Exec
