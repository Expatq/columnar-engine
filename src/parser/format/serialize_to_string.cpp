#include <parser/format/serialize_to_string.h>

#include <core/column.h>
#include <core/types.h>

#include <util/assert.h>
#include <util/calendar.h>
#include <util/int128.h>

#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <variant>

namespace Columnar::Parser {

std::string FormatDate(int32_t daysSinceEpoch) {
    std::time_t time = static_cast<std::time_t>(static_cast<int64_t>(daysSinceEpoch) * Calendar::kSecondsPerDay);
    std::tm* tm = std::gmtime(&time);

    if (!tm) {
        throw std::runtime_error("Cannot format date");
    }

    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%d");
    return ss.str();
}

std::string FormatTimestamp(int64_t secondsSinceEpoch) {
    std::time_t time = static_cast<std::time_t>(secondsSinceEpoch);
    std::tm* tm = std::gmtime(&time);

    if (!tm) {
        throw std::runtime_error("Cannot format timestamp");
    }

    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string FormatPhysicalCell(const Column& col, size_t row) {
    COLUMNAR_ASSERT(row < col.GetRowCount(),
                    "row out of range");
    return std::visit(
        Types::overloaded{
            [row](const std::vector<int16_t>& v) {
                return std::to_string(v[row]);
            },
            [row](const std::vector<int32_t>& v) {
                return std::to_string(v[row]);
            },
            [row](const std::vector<int64_t>& v) {
                return std::to_string(v[row]);
            },
            [row](const std::vector<uint8_t>& v) {
                return v[row] != 0 ? std::string{"true"} : std::string{"false"};
            },
            [row](const std::vector<std::string>& v) { return v[row]; },
            [row](const std::vector<Columnar::Int128>& v) { return Columnar::Int128ToString(v[row]); },
        },
        col.GetData());
}

std::string FormatColumn(const Column& col, size_t row, Types::LogicalType logical) {
    switch (logical) {
        case Types::LogicalType::DATE:
            return FormatDate(col.GetTypedData<int32_t>()[row]);
        case Types::LogicalType::TIMESTAMP:
            return FormatTimestamp(col.GetTypedData<int64_t>()[row]);
        default:
            return FormatPhysicalCell(col, row);
    }
}

}  // namespace Columnar::Parser
