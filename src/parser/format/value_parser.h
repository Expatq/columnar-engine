#pragma once

#include <core/types.h>
#include <string>

namespace Columnar::Parser {

Types::AnyColumnType ParseValue(const std::string& str,
                                Types::PhysicalType type);

std::string ValueToString(const Types::AnyColumnType& value,
                          Types::PhysicalType type);

}  // namespace Columnar::Parser