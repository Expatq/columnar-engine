#include <core/types.h>
#include <util/assert.h>

namespace Columnar::Types {

PhysicalType ToPhysical(LogicalType logical) {
    switch (logical) {
        case LogicalType::INT16:
            return PhysicalType::INT16;
        case LogicalType::INT32:
        case LogicalType::DATE:
            return PhysicalType::INT32;
        case LogicalType::INT64:
        case LogicalType::INT128:
        case LogicalType::TIMESTAMP:
            return PhysicalType::INT64;
        case LogicalType::BOOL:
            return PhysicalType::BOOL;
        case LogicalType::STRING:
            return PhysicalType::STRING;
        default:
            COLUMNAR_ASSERT(false, "ToPhysical: unknown LogicalType");
    }
}

std::string GetLogicalTypeName(LogicalType type) {
    switch (type) {
        case LogicalType::INT16:
            return "int16";
        case LogicalType::INT32:
            return "int32";
        case LogicalType::INT64:
            return "int64";
        case LogicalType::INT128:
            return "int128";
        case LogicalType::BOOL:
            return "bool";
        case LogicalType::STRING:
            return "string";
        case LogicalType::DATE:
            return "date";
        case LogicalType::TIMESTAMP:
            return "timestamp";
        default:
            COLUMNAR_ASSERT(false, "GetLogicalTypeName: unknown LogicalType");
    }
}

LogicalType ParseLogicalType(const std::string& type_name) {
    if (type_name == "int16")
        return LogicalType::INT16;
    if (type_name == "int32")
        return LogicalType::INT32;
    if (type_name == "int64")
        return LogicalType::INT64;
    if (type_name == "int128")
        return LogicalType::INT128;
    if (type_name == "bool")
        return LogicalType::BOOL;
    if (type_name == "string")
        return LogicalType::STRING;
    if (type_name == "date")
        return LogicalType::DATE;
    if (type_name == "timestamp")
        return LogicalType::TIMESTAMP;
    COLUMNAR_ASSERT(false, "ParseLogicalType: unknown type name (" + type_name + ")");
}

AnyColumnData CreateEmptyColumnData(LogicalType type) {
    switch (type) {
        case LogicalType::INT16:
            return std::vector<int16_t>();
        case LogicalType::INT32:
        case LogicalType::DATE:
            return std::vector<int32_t>();
        case LogicalType::INT64:
        case LogicalType::INT128:
        case LogicalType::TIMESTAMP:
            return std::vector<int64_t>();
        case LogicalType::BOOL:
            return std::vector<bool>();
        case LogicalType::STRING:
            return std::vector<std::string>();
        default:
            throw std::invalid_argument("Unknown data type");
    }
}

}  // namespace Columnar::Types
