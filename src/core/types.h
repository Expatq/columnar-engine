#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

#include <util/types_macro.h>

namespace Columnar::Types {

// Data types

enum class LogicalType : uint8_t {
    INT16 = 0,
    INT32 = 1,
    INT64 = 2,
    INT128 = 3,  // TODO: add int128 support
    BOOL = 4,
    STRING = 5,
    DATE = 6,      // TODO: add date support
    TIMESTAMP = 7  // TODO: add timestamp support
};

enum class PhysicalType : uint8_t {
    INT16 = 0,
    INT32 = 1,
    INT64 = 2,
    BOOL = 3,
    STRING = 4
};

PhysicalType ToPhysical(LogicalType logical);

using AnyColumnType =
    std::variant<int16_t, int32_t, int64_t, bool, std::string>;

using AnyColumnData = std::variant<std::vector<int16_t>, std::vector<int32_t>,
                                   std::vector<int64_t>, std::vector<bool>,
                                   std::vector<std::string>>;

template <typename... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

// File format constants

constexpr size_t kFileHeaderSize = 64;
constexpr size_t kRowGroupHeaderSize = 32;
constexpr size_t kChunkHeaderSize = 24;

// Helper functions

size_t GetTypeSize(LogicalType type);
std::string GetTypeName(LogicalType type);

bool IsFixedSize(LogicalType type);
LogicalType ParseDataType(const std::string& typeName);

inline size_t GetVariantIndex(PhysicalType physical_type) {
    return static_cast<size_t>(physical_type);
}

AnyColumnData CreateEmptyColumnData(PhysicalType type);

// Visitors for working with column data

struct GetSizeVisitor {
    DECLARE_CONST_VISITOR_FOR_ALL_TYPES(size_t);
};

struct ClearVisitor {
    DECLARE_MUTUABLE_VISITOR_FOR_ALL_TYPES(void);
};

struct ReserveVisitor {
    size_t capacity;
    DECLARE_MUTUABLE_VISITOR_FOR_ALL_TYPES(void);
};

}  // namespace Columnar::Types
