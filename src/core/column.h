#pragma once

#include <core/types.h>

#include <string>
#include <vector>

namespace Columnar {

class Column {
public:
    Column(Types::AnyColumnData data, Types::PhysicalType physical);

    Types::PhysicalType GetType() const;
    size_t GetRowCount() const;
    const Types::AnyColumnData& GetData() const;

    template <typename T>
    const std::vector<T>& GetTypedData() const;

    std::string GetValueAsString(size_t row) const;

private:
    void Validate() const;

private:
    Types::PhysicalType type_;  // Maybe set default to str
    Types::AnyColumnData data_;
};

}  // namespace Columnar
