#pragma once

#include "physical_type_for.h"

#include <core/column.h>
#include <core/row_group.h>
#include <core/schema.h>
#include <core/types.h>

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Columnar::Test {

template <typename T>
Column MakeColumn(std::vector<T> values) {
    return Column(Types::AnyColumnData{std::move(values)}, PhysicalTypeFor<T>());
}

Schema MakeSchema(std::initializer_list<std::pair<std::string, Types::LogicalType>> cols);

std::shared_ptr<RowGroup> MakeRowGroup(Schema schema, std::vector<Column> columns);

template <typename... Cols>
std::shared_ptr<RowGroup> MakeRowGroupOf(Schema schema, Cols&&... columns) {
    std::vector<Column> cols;
    cols.reserve(sizeof...(columns));
    (cols.emplace_back(std::forward<Cols>(columns)), ...);
    return MakeRowGroup(std::move(schema), std::move(cols));
}

}  // namespace Columnar::Test
