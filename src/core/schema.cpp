#include <core/schema.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include "core/types.h"
#include "util/assert.h"

namespace Columnar {

ColumnSchema::ColumnSchema(std::string name, Types::LogicalType logical)
    : name(std::move(name)),
      logical(logical),
      physical(Types::ToPhysical(logical)) {
    COLUMNAR_ASSERT(!this->name.empty(), "name cannot be empty");
}

Schema::Schema(std::vector<ColumnSchema> columns) {
    columns_.reserve(columns.size());
    for (auto& col : columns) {
        AddColumn(std::move(col));
    }
}

void Schema::AddColumn(ColumnSchema column) {
    COLUMNAR_ASSERT(!FindColumn(column.name).has_value(),
                    "duplicate column name '" + column.name + "'");
    columns_.push_back(std::move(column));
}

void Schema::AddColumn(const std::string& name, Types::LogicalType logical) {
    AddColumn(ColumnSchema{name, logical});
}

size_t Schema::GetColumnCount() const {
    return columns_.size();
}

const ColumnSchema& Schema::GetColumn(size_t index) const {
    COLUMNAR_ASSERT(index < columns_.size(),
                    "index " + std::to_string(index) + " >= " +
                        std::to_string(columns_.size()));
    return columns_[index];
}

std::string Schema::GetColumnName(size_t index) const {
    COLUMNAR_ASSERT(index < columns_.size(),
                    "index " + std::to_string(index) + " >= " +
                        std::to_string(columns_.size()));
    return columns_[index].name;
}

std::optional<size_t> Schema::FindColumn(const std::string& name) const {
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].name == name)
            return i;
    }
    return std::nullopt;
}

bool Schema::IsEmpty() const {
    return columns_.empty();
}

}  // namespace Columnar
