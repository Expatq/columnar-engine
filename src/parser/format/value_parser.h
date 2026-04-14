#pragma once

#include <core/types.h>
#include <string>

namespace Columnar::Parser {

Types::AnyPhysicalType ParseValue(const std::string& str,
                                  Types::PhysicalType type);

std::string ValueToString(const Types::AnyPhysicalType& value,
                          Types::PhysicalType type);

}  // namespace Columnar::Parser