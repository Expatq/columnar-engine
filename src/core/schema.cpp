#include <core/schema.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include "core/types.h"

namespace Columnar {

ColumnSchema::ColumnSchema(std::string name, Types::DataType type)
    : name(std::move(name)),
      type(type) {}

bool ColumnSchema::operator==(const ColumnSchema& other) const {
    return name == other.name && type == other.type;
}

bool ColumnSchema::operator!=(const ColumnSchema& other) const {
    return !(*this == other);
}

Schema::Schema(std::vector<ColumnSchema> columns)
    : Columns_(std::move(columns)) {}

void Schema::AddColumn(const std::string& name, Types::DataType type) {
    AddColumn(ColumnSchema(name, type));
}

void Schema::AddColumn(ColumnSchema column) {
    if (HasColumn(column.name)) {
        throw std::invalid_argument("Column already exists: " + column.name);
    }

    Columns_.push_back(std::move(column));
}

size_t Schema::GetColumnCount() const {
    return Columns_.size();
}

const ColumnSchema& Schema::GetColumn(size_t index) const {
    if (index >= Columns_.size()) {
        throw std::out_of_range(
            "Column index out of range: " + std::to_string(index) +
            " >= " + std::to_string(Columns_.size()));
    }

    return Columns_[index];
}

std::optional<size_t> Schema::FindColumn(const std::string& name) const {
    for (size_t i = 0; i < Columns_.size(); ++i) {
        if (Columns_[i].name == name) {
            return i;
        }
    }

    return std::nullopt;
}

bool Schema::HasColumn(const std::string& name) const {
    return FindColumn(name).has_value();
}

bool Schema::IsEmpty() const {
    return Columns_.empty();
}

bool Schema::operator==(const Schema& other) const {
    if (other.GetColumnCount() != Columns_.size()) {
        return false;
    }

    for (size_t i = 0; i < GetColumnCount(); ++i) {
        if (Columns_[i] != other.GetColumn(i)) {
            return false;
        }
    }

    return true;
}

bool Schema::operator!=(const Schema& other) const {
    return !(*this == other);
}

bool Schema::IsValid() const {
    if (Columns_.empty()) {
        return false;
    }

    std::unordered_set<std::string> names;
    for (const auto& column : Columns_) {
        if (column.name.empty()) {
            return false;
        }

        if (names.contains(column.name)) {
            return false;
        }

        names.insert(column.name);
    }

    return true;
}

}  // namespace Columnar