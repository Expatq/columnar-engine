#include "row_group.h"
#include "column.h"
#include "schema.h"

#include <util/assert.h>

#include <iterator>
#include <utility>

namespace Columnar {

RowGroup::RowGroup(Schema schema, std::vector<Column> columns)
    : schema_(std::move(schema)),
      columns_(std::make_move_iterator(columns.begin()), std::make_move_iterator(columns.end())) {
    Validate();
    rowCount_ = columns_.empty() ? 0 : columns_[0].GetRowCount();
}

RowGroup::RowGroup(RowGroup&& other) noexcept
    : schema_(std::move(other.schema_)),
      columns_(std::move(other.columns_)),
      rowCount_(std::exchange(other.rowCount_, 0)) {
}

RowGroup& RowGroup::operator=(RowGroup&& other) noexcept {
    schema_ = std::move(other.schema_);
    columns_ = std::move(other.columns_);
    rowCount_ = std::exchange(other.rowCount_, 0);
    return *this;
}

void RowGroup::Validate() const {
    COLUMNAR_ASSERT(!columns_.empty(),
                    "must have at least one column");
    COLUMNAR_ASSERT(schema_.GetColumnCount() == columns_.size(),
                    "schema and columns count mismatch");

    const size_t expected = columns_[0].GetRowCount();
    for (size_t i = 1; i < columns_.size(); ++i) {
        COLUMNAR_ASSERT(
            columns_[i].GetRowCount() == expected,
            "row count mismatch in column: " + schema_.GetColumnName(i));
        COLUMNAR_ASSERT(
            columns_[i].GetType() == schema_.GetColumn(i).physical,
            "type mismatch in column " + schema_.GetColumnName(i));
    }
}

size_t RowGroup::GetColumnCount() const {
    return columns_.size();
}

size_t RowGroup::GetRowCount() const {
    return rowCount_;
}

const Schema& RowGroup::GetSchema() const {
    return schema_;
}

const Column& RowGroup::GetColumn(size_t index) const {
    COLUMNAR_ASSERT(index < columns_.size(), "column index out of range");
    return columns_[index];
}

Column& RowGroup::GetColumn(size_t index) {
    COLUMNAR_ASSERT(index < columns_.size(), "column index out of range");
    return columns_[index];
}

const Column* RowGroup::FindColumn(const std::string& name) const {
    auto idx = schema_.FindColumn(name);
    COLUMNAR_ASSERT(idx.has_value(),
                    "unknown column name: " + name);
    return idx ? &columns_[*idx] : nullptr;
}

Column* RowGroup::FindColumn(const std::string& name) {
    auto idx = schema_.FindColumn(name);
    COLUMNAR_ASSERT(idx.has_value(),
                    "unknown column name: " + name);
    return idx ? &columns_[*idx] : nullptr;
}

}  // namespace Columnar
