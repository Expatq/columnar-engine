#pragma once

#include <core/row_group.h>
#include <core/schema.h>
#include <core/types.h>

#include <util/assert.h>

namespace Columnar::Exec {

class RowGroupBuilder {
public:
    explicit RowGroupBuilder(Schema schema);

    template <typename T>
    void Append(size_t colIdx, T value) {
        COLUMNAR_ASSERT(colIdx < columns_.size(), "result column index out of range");
        auto& column = std::get<std::vector<T>>(columns_[colIdx]);
        column.push_back(std::move(value));
    }

    RowGroup Finish();

private:
    Schema schema_;
    std::vector<Types::AnyColumnData> columns_;

};

}  // namespace Columnar::Exec
