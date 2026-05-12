#pragma once

#include <cstddef>
#include <cstdint>

namespace Columnar::IO {

constexpr uint8_t kMagicBytes[4] = {'I', 'Y', 'X', 0x01};
constexpr size_t kMagicSize = 4;
constexpr size_t kHeaderSize = 64;

}  // namespace Columnar::IO
