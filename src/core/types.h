#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

#include <util/int128.h>
#include <util/types_macro.h>

namespace Columnar::Types {

// Data types

enum class LogicalType : uint8_t {
    INT16 = 0,
    INT32 = 1,
    INT64 = 2,
    INT128 = 3,
    BOOL = 4,
    STRING = 5,
    DATE = 6,
    TIMESTAMP = 7
};

enum class PhysicalType : uint8_t {
    INT16 = 0,
    INT32 = 1,
    INT64 = 2,
    INT128 = 3,
    BOOL = 4,
    STRING = 5
};

PhysicalType ToPhysical(LogicalType logical);

inline size_t GetPhysVariantIndex(PhysicalType physical_type) {
    return static_cast<size_t>(physical_type);
}

using AnyPhysicalType =
    std::variant<int16_t, int32_t, int64_t, Int128, uint8_t, std::string>;

using AnyColumnData = std::variant<std::vector<int16_t>, std::vector<int32_t>,
                                   std::vector<int64_t>, std::vector<Int128>, std::vector<uint8_t>,
                                   std::vector<std::string>>;

template <typename... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

// Helper functions

std::string GetLogicalTypeName(LogicalType type);
LogicalType ParseLogicalType(const std::string& typeName);
AnyColumnData CreateEmptyColumnData(PhysicalType type);

// Visitors for working with column data

struct GetSizeVisitor {
    template <typename T>
    std::size_t operator()(const std::vector<T>& vec) const {
        return vec.size();
    }
};

}  // namespace Columnar::Types
