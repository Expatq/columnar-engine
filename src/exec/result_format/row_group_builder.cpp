#include "row_group_builder.h"

namespace Columnar::Exec {

RowGroupBuilder::RowGroupBuilder(Schema schema)
    : schema_(std::move(schema)) {
}

RowGroup RowGroupBuilder::Finish() {
    std::vector<Column> result;
    result.reserve(columns_.size());

    for (size_t i = 0; i < columns_.size(); ++i) {
        result.emplace_back(std::move(columns_[i]), schema_.GetColumn(i).physical);
    }

    return RowGroup(std::move(schema_), std::move(result));
}

}  // namespace Columnar::Exec
