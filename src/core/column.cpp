#include <core/column.h>
#include <core/types.h>

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

}  // namespace Columnar
