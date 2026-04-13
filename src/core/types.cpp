#include <core/types.h>

namespace Columnar::Types {

size_t GetTypeSize(LogicalType type) {
    switch (type) {
        case LogicalType::INT16:
            return sizeof(int16_t);
        case LogicalType::INT32:
            return sizeof(int32_t);
        case LogicalType::INT64:
            return sizeof(int64_t);
        case LogicalType::INT128:
            return sizeof(int64_t);  // TODO: fix sizeof int128
        case LogicalType::BOOL:
            return sizeof(bool);
        case LogicalType::DATE:
            return sizeof(int32_t);
        case LogicalType::TIMESTAMP:
            return sizeof(int64_t);
        case LogicalType::STRING:
            return 0;
        default:
            throw std::invalid_argument("Unknown data type");
    }
}

std::string GetTypeName(LogicalType type) {
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
            return "unknown";
    }
}

bool IsFixedSize(LogicalType type) {
    return type != LogicalType::STRING;
}

LogicalType ParseDataType(const std::string& type_name) {
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
    throw std::invalid_argument("Unknown type name: " + type_name);
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

// Visitor implementations
IMPL_CONST_VISITOR_FOR_ALL_TYPES(GetSizeVisitor, size_t, return data.size();)
IMPL_MUTABLE_VISITOR_FOR_ALL_TYPES(ClearVisitor, void, data.clear();)
IMPL_MUTABLE_VISITOR_FOR_ALL_TYPES(ReserveVisitor, void,
                                   data.reserve(capacity);)

}  // namespace Columnar::Types