#include "data_builders.h"

#include <core/schema.h>

#include <string>

namespace Columnar::Test {

Schema MakeSchema(std::initializer_list<std::pair<std::string, Types::LogicalType>> cols) {
    Schema schema;
    for (const auto& [name, logical] : cols) {
        schema.AddColumn(name, logical);
    }
    return schema;
}

std::shared_ptr<RowGroup> MakeRowGroup(Schema schema, std::vector<Column> columns) {
    return std::make_shared<RowGroup>(std::move(schema), std::move(columns));
}

}  // namespace Columnar::Test
