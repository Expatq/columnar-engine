#include <core/types.h>

namespace Columnar::Types {

size_t GetTypeSize(DataType type) {
    switch (type) {
        case DataType::INT16:
            return sizeof(int16_t);
        case DataType::INT32:
            return sizeof(int32_t);
        case DataType::INT64:
            return sizeof(int64_t);
        case DataType::INT128:
            return sizeof(int64_t);  // TODO: fix sizeof int128
        case DataType::BOOL:
            return sizeof(bool);
        case DataType::DATE:
            return sizeof(int32_t);
        case DataType::TIMESTAMP:
            return sizeof(int64_t);
        case DataType::STRING:
            return 0;
        default:
            throw std::invalid_argument("Unknown data type");
    }
}

std::string GetTypeName(DataType type) {
    switch (type) {
        case DataType::INT16:
            return "int16";
        case DataType::INT32:
            return "int32";
        case DataType::INT64:
            return "int64";
        case DataType::INT128:
            return "int128";
        case DataType::BOOL:
            return "bool";
        case DataType::STRING:
            return "string";
        case DataType::DATE:
            return "date";
        case DataType::TIMESTAMP:
            return "timestamp";
        default:
            return "unknown";
    }
}

bool IsFixedSize(DataType type) {
    return type != DataType::STRING;
}

DataType ParseDataType(const std::string& type_name) {
    if (type_name == "int16")
        return DataType::INT16;
    if (type_name == "int32")
        return DataType::INT32;
    if (type_name == "int64")
        return DataType::INT64;
    if (type_name == "int128")
        return DataType::INT128;
    if (type_name == "bool")
        return DataType::BOOL;
    if (type_name == "string")
        return DataType::STRING;
    if (type_name == "date")
        return DataType::DATE;
    if (type_name == "timestamp")
        return DataType::TIMESTAMP;
    throw std::invalid_argument("Unknown type name: " + type_name);
}

size_t GetVariantIndex(DataType type) {
    switch (type) {
        case DataType::INT16:
            return 0;
        case DataType::INT32:
        case DataType::DATE:
            return 1;
        case DataType::INT64:
        case DataType::INT128:
        case DataType::TIMESTAMP:
            return 2;
        case DataType::BOOL:
            return 3;
        case DataType::STRING:
            return 4;
        default:
            throw std::invalid_argument("Unknown data type");
    }
}

AnyColumnData CreateEmptyColumnData(DataType type) {
    switch (type) {
        case DataType::INT16:
            return std::vector<int16_t>();
        case DataType::INT32:
        case DataType::DATE:
            return std::vector<int32_t>();
        case DataType::INT64:
        case DataType::INT128:
        case DataType::TIMESTAMP:
            return std::vector<int64_t>();
        case DataType::BOOL:
            return std::vector<bool>();
        case DataType::STRING:
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