#pragma once

#include "assert.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace Columnar {

struct ByteBuffer {
    std::vector<uint8_t> data;

    void Append(const void* src, size_t n) {
        const auto* p = static_cast<const uint8_t*>(src);
        data.insert(data.end(), p, p + n);
    }

    template <typename T>
    void Append(const T& v) { Append(&v, sizeof(v)); }

    template <typename T>
    void PatchAt(size_t pos, const T& v) {
        COLUMNAR_ASSERT(pos + sizeof(v) <= data.size(), "patch exceeds data size");
        std::memcpy(data.data() + pos, &v, sizeof(v));
    }

    size_t size() const { return data.size(); }
    void reserve(size_t n) { data.reserve(n); }
};

}  // namespace Columnar
