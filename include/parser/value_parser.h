#pragma once

#include <core/types.h>
#include <string>

namespace Columnar::Parser {

Types::AnyColumnType ParseValue(const std::string& str, Types::DataType type);

std::string ValueToString(const Types::AnyColumnType& value,
                          Types::DataType type);

}  // namespace Columnar::Parser