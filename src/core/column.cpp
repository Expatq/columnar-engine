#include <core/column.h>
#include <core/types.h>

#include <util/assert.h>

#include <variant>

namespace Columnar {

Column::Column(Types::AnyColumnData data, Types::PhysicalType type)
    : type_(type),
      data_(std::move(data)) {
    Validate();
}

void Column::Validate() const {
    COLUMNAR_ASSERT(data_.index() == Types::GetPhysVariantIndex(type_),
                    "data variant does not match type");
}

Types::PhysicalType Column::GetType() const {
    return type_;
}

size_t Column::GetRowCount() const {
    return std::visit(Types::GetSizeVisitor{}, data_);
}

const Types::AnyColumnData& Column::GetData() const {
    return data_;
}

template <typename T>
const std::vector<T>& Column::GetTypedData() const {
    COLUMNAR_ASSERT(std::holds_alternative<std::vector<T>>(data_),
                    "Column::GetTypedData: type mismatch");
    return std::get<std::vector<T>>(data_);
}

std::string Column::GetValueAsString(size_t row) const {
    COLUMNAR_ASSERT(row < GetRowCount(),
                    "Column::GetValueAsString: row out of range");

    return std::visit(
        Types::overloaded{
            [row](const std::vector<int16_t>& vec) {
                return std::to_string(vec[row]);
            },
            [row](const std::vector<int32_t>& vec) {
                return std::to_string(vec[row]);
            },
            [row](const std::vector<int64_t>& vec) {
                return std::to_string(vec[row]);
            },
            [row](const std::vector<bool>& vec) {
                return vec[row] ? std::string{"true"} : std::string{"false"};
            },
            [row](const std::vector<std::string>& vec) { return vec[row]; }},
        data_);
}

}  // namespace Columnar
