#include "column_ref.h"

#include <exec/core/exec_batch.h>
#include <exec/core/selection_vector.h>

#include <core/types.h>

#include <util/assert.h>

#include <type_traits>
#include <utility>

namespace Columnar::Exec {

ColumnSpan ColumnRefExpression::EvaluateColumn(const ExecBatch& input, EvalState& state) const {
    COLUMNAR_ASSERT(input.rowGroup != nullptr, "ColumnRef needs RowGroup");
    const Column* col = input.rowGroup->FindColumn(column_);
    COLUMNAR_ASSERT(col != nullptr, "column not found: " + column_);

    if (!input.has_selection) {
        return std::visit([](const auto& vec) -> ColumnSpan {
            return std::span{vec.data(), vec.size()};
        },col->GetData());
    }

    return std::visit([&](const auto& src) -> ColumnSpan {
        using T = typename std::decay_t<decltype(src)>::value_type;
        auto out = state.ResizeBuffer<T>(input.selection.Size());
        RowId i = 0;
        for (RowId row : input.selection.Rows()) {
            out[i++] = src[row];
        }
        return std::span<const T>{out.data(), out.size()};
    },col->GetData());
}

Types::AnyPhysicalType ColumnRefExpression::EvaluateScalar(const ExecBatch& input, RowId row) const {
    COLUMNAR_ASSERT(input.rowGroup != nullptr, "ColumnRef needs RowGroup");
    const Column* col = input.rowGroup->FindColumn(column_);
    COLUMNAR_ASSERT(col != nullptr, "column not found: " + column_);

    switch (Types::ToPhysical(type_)) {
        case Types::PhysicalType::INT16:
            return col->GetTypedData<int16_t>()[row];
        case Types::PhysicalType::INT32:
            return col->GetTypedData<int32_t>()[row];
        case Types::PhysicalType::INT64:
            return col->GetTypedData<int64_t>()[row];
        case Types::PhysicalType::INT128:
            return col->GetTypedData<Int128>()[row];
        case Types::PhysicalType::BOOL:
            return col->GetTypedData<uint8_t>()[row];
        case Types::PhysicalType::STRING:
            return col->GetTypedData<std::string>()[row];
    }
    std::unreachable();
}

}  // namespace Columnar::Exec
