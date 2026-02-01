#include <core/types.h>
#include <parser/value_parser.h>
#include <util/str.h>

#include <charconv>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace Columnar::Parser {

namespace {

// TODO: generalize implementation for all parse int

// Parsers

int16_t ParseInt16(const std::string& str) {
    std::string stripped = str::strip(str);
    if (stripped.empty()) {
        throw std::invalid_argument("Cannot parse empty string as int16");
    }

    int16_t result;
    auto [ptr, err_code] = std::from_chars(
        stripped.data(), stripped.data() + stripped.size(), result);

    if (err_code != std::errc{} || ptr != (stripped.data() + stripped.size())) {
        throw std::invalid_argument("Cannot parse '" + str + "' as int16");
    }

    return result;
}

int32_t ParseInt32(const std::string& str) {
    std::string stripped = str::strip(str);
    if (stripped.empty()) {
        throw std::invalid_argument("Cannot parse empty string as int32");
    }

    int32_t result;
    auto [ptr, err_code] = std::from_chars(
        stripped.data(), stripped.data() + stripped.size(), result);

    if (err_code != std::errc{} || ptr != (stripped.data() + stripped.size())) {
        throw std::invalid_argument("Cannot parse '" + str + "' as int32");
    }

    return result;
}

int64_t ParseInt64(const std::string& str) {
    std::string stripped = str::strip(str);
    if (stripped.empty()) {
        throw std::invalid_argument("Cannot parse empty string as int64");
    }

    int64_t result;
    auto [ptr, err_code] = std::from_chars(
        stripped.data(), stripped.data() + stripped.size(), result);

    if (err_code != std::errc{} || ptr != (stripped.data() + stripped.size())) {
        throw std::invalid_argument("Cannot parse '" + str + "' as int64");
    }

    return result;
}

[[maybe_unused]] int64_t ParseInt128(  // TODO: implement int128 support
    const std::string& str) {
    throw std::runtime_error("not implemented yet");
    return str[0];  // some garbage for clangd
}

bool ParseBool(const std::string& str) {
    std::string pretty_str = str::tolower(str::strip(str));

    if (pretty_str == "true") {
        return true;
    }

    if (pretty_str == "false") {
        return false;
    }

    throw std::invalid_argument("Cannot parse '" + str + "' as bool");
}

/**
 *   @brief Parse date in format YYYY-MM-DD
 *   @return Days since unix epoch (1970-01-01)
 */
int32_t ParseDate(const std::string& str) {
    std::string stripped = str::strip(str);

    std::tm tm = {};
    std::istringstream ss(stripped);
    ss >> std::get_time(&tm, "%Y-%m-%d");

    if (ss.fail()) {
        throw std::invalid_argument("Cannot parse '" + str +
                                    "' as date (expected YYYY-MM-DD)");
    }

    tm.tm_hour = 12;
    std::time_t time = std::mktime(&tm);
    if (time == -1) {
        throw std::invalid_argument("Invalid date: " + str);
    }

    constexpr int64_t kSecondsPerDay = 86400;
    return static_cast<int32_t>(time / kSecondsPerDay);
}

/**
 *   @brief Parse timestamp in format "YYYY-MM-DD HH:MM:SS"
 *   @return Seconds since unix epoch (1970-01-01)
*/
int64_t ParseTimestamp(const std::string& str) {
    std::string stripped = str::strip(str);

    std::tm tm = {};
    std::istringstream ss(stripped);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

    if (ss.fail()) {
        throw std::invalid_argument(
            "Cannot parse '" + str +
            "' as timestamp (expected YYYY-MM-DD HH:MM:SS)");
    }

    std::time_t time = std::mktime(&tm);
    if (time == -1) {
        throw std::invalid_argument("Invalid timestamp: " + str);
    }

    return static_cast<int64_t>(time);
}

// Formatters:

std::string FormatDate(int32_t daysSinceEpoch) {
    constexpr int64_t kSecondsPerDay = 86400;
    std::time_t time =
        static_cast<std::time_t>(daysSinceEpoch) * kSecondsPerDay;
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

}  // namespace

Types::AnyColumnType ParseValue(const std::string& str, Types::DataType type) {
    switch (type) {
        case Types::DataType::INT16:
            return ParseInt16(str);
        case Types::DataType::INT32:
            return ParseInt32(str);
        case Types::DataType::INT64:
            return ParseInt64(str);
        case Types::DataType::BOOL:
            return ParseBool(str);
        case Types::DataType::STRING:
            return str;
        case Types::DataType::DATE:
            return ParseDate(str);
        case Types::DataType::TIMESTAMP:
            return ParseTimestamp(str);
        case Types::DataType::INT128:
            throw std::invalid_argument("INT128 parsing not implemented");
        default:
            throw std::invalid_argument("Unsupported data type for parsing");
    }
}

std::string ValueToString(const Types::AnyColumnType& value,
                          Types::DataType type) {
    return std::visit(
        Types::overloaded{[](int16_t v) { return std::to_string(v); },
                          [type](int32_t v) {
                              if (type == Types::DataType::DATE) {
                                  return FormatDate(v);
                              }
                              return std::to_string(v);
                          },
                          [type](int64_t v) {
                              if (type == Types::DataType::TIMESTAMP) {
                                  return FormatTimestamp(v);
                              }
                              return std::to_string(v);
                          },
                          [](bool v) {
                              return v ? std::string("true")
                                       : std::string("false");
                          },
                          [](const std::string& v) { return v; }},
        value);
}

}  // namespace Columnar::Parser