#pragma once

#include <core/types.h>
#include <exec/interface/expression.h>
#include <stdint.h>
#include "util/int128.h"

namespace Columnar::Exec {
/*
Arena allocator for inline keys that exceed InlineKey::kPrefixBytes
TODO: maybe add deallocate method
*/
struct KeysArena {
    std::vector<char> buf;
    uint32_t pos = 0;

    uint32_t Push(const char* src, size_t n) {
        if (pos + n > buf.size()) {
            buf.resize(std::max(buf.size() * 2, pos + n));
        }
        const uint32_t offset = pos;
        std::memcpy(buf.data() + pos, src, n);
        pos += static_cast<uint32_t>(n);
        return offset;
    }

    void Reset() {
        pos = 0;
    }

    const char* Data() const {
        return buf.data();
    }
};

/*
If  len <= kPrefixBytes - all key in prefix
    len > kPrefixBytes - kPrefixBytes stored in 'prefix' and all key stored in arena 
    the first kPrefixBytes are stored twice
*/
struct InlineKey {
    static constexpr size_t kPrefixBytes = 32;
    uint32_t hash = 0;
    uint32_t len = 0;
    uint32_t arenaOffset = 0;
    char prefix[kPrefixBytes]{};

    bool IsInline() const {
        return len <= kPrefixBytes;
    }
};

struct InlineKeyHash {
    size_t operator()(const InlineKey& key) const noexcept {
        return key.hash;
    }
};

struct InlineKeyEq {
    const KeysArena* arena;
    bool operator()(const InlineKey& lhs, const InlineKey& rhs) const noexcept {
        if (lhs.hash != rhs.hash || lhs.len != rhs.len) {
            return false;
        }
        const size_t cmpLen = std::min<size_t>(lhs.len, InlineKey::kPrefixBytes);
        if (std::memcmp(lhs.prefix, rhs.prefix, cmpLen) != 0) {
            return false;
        }
        if (lhs.len > InlineKey::kPrefixBytes) {
            return std::memcmp(arena->Data() + lhs.arenaOffset, arena->Data() + rhs.arenaOffset, lhs.len) == 0;
        }
        return true;
    }
};

struct Int128Hash {
    size_t operator()(Int128 value) const noexcept {
        const uint64_t lo = static_cast<uint64_t>(value);
        const uint64_t hi = static_cast<uint64_t>(value >> 64);
        return lo ^ (hi * 0x9e3779b97f4a7c15ULL);
    }
};

enum class KeyMode {
    Int64,
    Int128,
    Inline
};

}  // namespace Columnar::Exec
