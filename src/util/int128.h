#pragma once

#include <algorithm>
#include <string>
#include <type_traits>

namespace Columnar {

using Int128 = __int128;
using UInt128 = unsigned __int128;

template <typename T>
concept T128 = std::is_same_v<T, Int128> || std::is_same_v<T, UInt128>;

template <T128 I128>
inline std::string Int128ToString(I128 value) {
    if (value == 0) {
        return "0";
    }
    std::string result;
    bool isNegative = value < 0;
    UInt128 uvalue = isNegative ? -static_cast<UInt128>(value) : static_cast<UInt128>(value);

    while (uvalue > 0) {
        result += static_cast<char>('0' + static_cast<int>(uvalue % 10));
        uvalue /= 10;
    }
    if (isNegative) {
        result += '-';
    }
    std::reverse(result.begin(), result.end());
    return result;
}

}  // namespace Columnar
