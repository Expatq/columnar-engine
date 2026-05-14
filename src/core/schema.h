#pragma once

#include <core/types.h>

#include <absl/container/inlined_vector.h>

#include <optional>
#include <string>
#include <vector>

namespace Columnar {

struct ColumnSchema {
    std::string name;
    Types::LogicalType logical;
    Types::PhysicalType physical;

    ColumnSchema(std::string name, Types::LogicalType logical);

    bool operator==(const ColumnSchema& other) const = default;
};

class Schema {
public:
    using const_iterator = absl::InlinedVector<ColumnSchema, 8>::const_iterator;

    Schema() = default;
    explicit Schema(std::vector<ColumnSchema> columns);

    void AddColumn(ColumnSchema column);
    void AddColumn(const std::string& name, Types::LogicalType logical);

    size_t GetColumnCount() const;
    const ColumnSchema& GetColumn(size_t index) const;
    std::string GetColumnName(size_t index) const;
    std::optional<size_t> FindColumn(const std::string& name) const;

    bool IsEmpty() const;

    const_iterator begin() const { return columns_.begin(); }

    const_iterator end() const { return columns_.end(); }

    bool operator==(const Schema& other) const = default;

private:
    absl::InlinedVector<ColumnSchema, 8> columns_;
};

}  // namespace Columnar
