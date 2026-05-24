#include "value_parser.h"

#include <parser/format/serialize_to_string.h>
#include <core/types.h>
#include <util/calendar.h>
#include <util/int128.h>
#include <util/str.h>

#include <charconv>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace Columnar::Parser {

namespace {

template <typename T>
T ParseInt(std::string_view sv) {
    sv = str::strip(sv);
    if (sv.empty())
        throw std::invalid_argument("cannot parse empty string as int");
    T result;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), result);
    if (ec != std::errc{} || ptr != sv.data() + sv.size())
        throw std::invalid_argument("cannot parse '" + std::string(sv) + "' as int");
    return result;
}

uint8_t ParseBool(std::string_view sv) {
    sv = str::strip(sv);
    if (sv == "true")  return 1;
    if (sv == "false") return 0;
    throw std::invalid_argument("cannot parse '" + std::string(sv) + "' as bool");
}

int32_t ParseDate(std::string_view sv) {
    sv = str::strip(sv);
    if (sv.size() != 10)
        throw std::invalid_argument("cannot parse '" + std::string(sv) + "' as date");
    const int y = (sv[0]-'0')*1000 + (sv[1]-'0')*100 + (sv[2]-'0')*10 + (sv[3]-'0');
    const int m = (sv[5]-'0')*10   + (sv[6]-'0');
    const int d = (sv[8]-'0')*10   + (sv[9]-'0');
    return Calendar::GregorianToEpochDays(y, m, d);
}

int64_t ParseTimestamp(std::string_view sv) {
    sv = str::strip(sv);
    if (sv.size() != 19)
        throw std::invalid_argument("cannot parse '" + std::string(sv) + "' as timestamp");
    const int y  = (sv[0]-'0')*1000 + (sv[1]-'0')*100 + (sv[2]-'0')*10 + (sv[3]-'0');
    const int mo = (sv[5]-'0')*10   + (sv[6]-'0');
    const int d  = (sv[8]-'0')*10   + (sv[9]-'0');
    const int h  = (sv[11]-'0')*10  + (sv[12]-'0');
    const int mi = (sv[14]-'0')*10  + (sv[15]-'0');
    const int s  = (sv[17]-'0')*10  + (sv[18]-'0');
    return Calendar::GregorianToEpochDays(y, mo, d) * Calendar::kSecondsPerDay
           + h * Calendar::kSecondsPerHour + mi * Calendar::kSecondsPerMinute + s;
}

}  // namespace

Types::AnyPhysicalType ParseValue(std::string_view sv, Types::LogicalType type) {
    switch (type) {
        case Types::LogicalType::INT16:     return ParseInt<int16_t>(sv);
        case Types::LogicalType::INT32:     return ParseInt<int32_t>(sv);
        case Types::LogicalType::INT64:     return ParseInt<int64_t>(sv);
        case Types::LogicalType::BOOL:      return ParseBool(sv);
        case Types::LogicalType::STRING:    return std::string(sv);
        case Types::LogicalType::DATE:      return ParseDate(sv);
        case Types::LogicalType::TIMESTAMP: return ParseTimestamp(sv);
        case Types::LogicalType::INT128:
            throw std::invalid_argument("INT128 parsing not implemented");
        default:
            throw std::invalid_argument("unsupported type");
    }
}

std::string ValueToString(const Types::AnyPhysicalType& value, Types::LogicalType type) {
    return std::visit(
        Types::overloaded{
            [](int16_t v)  { return std::to_string(v); },
            [type](int32_t v) {
                return type == Types::LogicalType::DATE
                    ? FormatDate(v) : std::to_string(v);
            },
            [type](int64_t v) {
                return type == Types::LogicalType::TIMESTAMP
                    ? FormatTimestamp(v) : std::to_string(v);
            },
            [](uint8_t v)  { return v ? std::string("true") : std::string("false"); },
            [](const std::string& v) { return v; },
            [](Columnar::Int128 v) { return Columnar::Int128ToString(v); },
        },
        value);
}

}  // namespace Columnar::Parser
