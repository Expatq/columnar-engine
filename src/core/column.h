#pragma once

#include <core/types.h>
#include <util/assert.h>

#include <variant>
#include <vector>

namespace Columnar {

class Column {
public:
    Column(Types::AnyColumnData data, Types::PhysicalType physical);

    Types::PhysicalType GetType() const;
    size_t GetRowCount() const;
    const Types::AnyColumnData& GetData() const;

    template <typename T>
    const std::vector<T>& GetTypedData() const {
        COLUMNAR_ASSERT(std::holds_alternative<std::vector<T>>(data_),
                        "type mismatch");
        return std::get<std::vector<T>>(data_);
    }

private:
    void Validate() const;

private:
    Types::PhysicalType type_;
    Types::AnyColumnData data_;
};

}  // namespace Columnar
