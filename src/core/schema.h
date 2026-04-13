#pragma once

#include <core/types.h>

#include <optional>
#include <string>
#include <vector>

namespace Columnar {

struct ColumnSchema {
    std::string name;
    Types::PhysicalType type;

    ColumnSchema(std::string name, Types::PhysicalType type);

    bool operator==(const ColumnSchema& other) const = default;
};

class Schema {
public:
    using const_iterator = std::vector<ColumnSchema>::const_iterator;

    Schema() = default;
    explicit Schema(std::vector<ColumnSchema> columns);

    void AddColumn(ColumnSchema column);
    void AddColumn(const std::string& name, Types::PhysicalType type);

    size_t GetColumnCount() const;
    const ColumnSchema& GetColumn(size_t index) const;
    std::string GetColumnName(size_t index) const;
    std::optional<size_t> FindColumn(const std::string& name) const;

    const_iterator begin() const { return columns_.begin(); }

    const_iterator end() const { return columns_.end(); }

    bool operator==(const Schema& other) const = default;

private:
    std::vector<ColumnSchema> columns_;
};

}  // namespace Columnar
