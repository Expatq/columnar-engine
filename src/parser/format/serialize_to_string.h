#pragma once

#include <core/types.h>
#include <core/column.h>

#include <cstddef>
#include <string>

namespace Columnar::Parser {

std::string FormatDate(int32_t daysSinceEpoch);
std::string FormatTimestamp(int64_t secondsSinceEpoch);

std::string FormatPhysicalCell(const Column& col, size_t row);

std::string FormatColumn(const Column& col, size_t row, Types::LogicalType logical);

}  // namespace Columnar::Parser
