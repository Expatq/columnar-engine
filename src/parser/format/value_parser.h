#pragma once

#include <parser/format/serialize_to_string.h>

#include <core/types.h>
#include <string>

namespace Columnar::Parser {

Types::AnyPhysicalType ParseValue(const std::string& str,
                                  Types::LogicalType type);

std::string ValueToString(const Types::AnyPhysicalType& value,
                          Types::LogicalType type);

}  // namespace Columnar::Parser
