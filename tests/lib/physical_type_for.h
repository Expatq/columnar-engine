#pragma once

#include <core/types.h>

#include <util/int128.h>

#include <cstdint>
#include <string>
#include <type_traits>

namespace Columnar::Test {

template <typename T>
constexpr Types::PhysicalType PhysicalTypeFor() {
    if constexpr (std::is_same_v<T, int16_t>) {
        return Types::PhysicalType::INT16;
    } else if constexpr (std::is_same_v<T, int32_t>) {
        return Types::PhysicalType::INT32;
    } else if constexpr (std::is_same_v<T, int64_t>) {
        return Types::PhysicalType::INT64;
    } else if constexpr (std::is_same_v<T, Int128>) {
        return Types::PhysicalType::INT128;
    } else if constexpr (std::is_same_v<T, uint8_t>) {
        return Types::PhysicalType::BOOL;
    } else if constexpr (std::is_same_v<T, std::string>) {
        return Types::PhysicalType::STRING;
    } else {
        static_assert(sizeof(T) == 0, "unsupported physical type");
    }
}

}  // namespace Columnar::Test
