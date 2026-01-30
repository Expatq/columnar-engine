#pragma once

#include <optional>
#include <string>
#include <vector>

#include <core/types.h>

namespace Columnar {

struct ColumnSchema {
    std::string name;
    Types::DataType type;

    ColumnSchema() = default;
    ColumnSchema(std::string name, Types::DataType type);

    bool operator==(const ColumnSchema& other) const;
    bool operator!=(const ColumnSchema& other) const;
};

class Schema {
public:
    using iterator = std::vector<ColumnSchema>::iterator;
    using const_iterator = std::vector<ColumnSchema>::const_iterator;

    Schema() = default;
    explicit Schema(std::vector<ColumnSchema> columns);

    void AddColumn(const std::string& name, Types::DataType type);
    void AddColumn(ColumnSchema column);

    size_t GetColumnCount() const;
    const ColumnSchema& GetColumn(size_t index) const;

    std::optional<size_t> FindColumn(const std::string& name) const;
    bool HasColumn(const std::string& name) const;

    bool IsEmpty() const;

    iterator begin() { return Columns_.begin(); }

    iterator end() { return Columns_.end(); }

    const_iterator begin() const { return Columns_.begin(); }

    const_iterator end() const { return Columns_.end(); }

    const_iterator cbegin() const { return Columns_.cbegin(); }

    const_iterator cend() const { return Columns_.cend(); }

    bool operator==(const Schema& other) const;
    bool operator!=(const Schema& other) const;

    bool IsValid() const;

private:
    std::vector<ColumnSchema> Columns_;
};

}  // namespace Columnar