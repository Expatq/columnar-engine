#include "serializer.h"

#include "core/types.h"
#include "exec/aggregate/groupby/inline_key.h"
#include "util/int128.h"

namespace Columnar::Exec {

size_t GroupByKeySerializer::PackedSize(const std::vector<GroupByKey>& keyDefs) {
    size_t total = 0;
    for (const auto& keydef : keyDefs) {
        switch (Types::ToPhysical(keydef.expr->ResultType())) {
            case Types::PhysicalType::BOOL:
                total += sizeof(uint8_t);
                break;
            case Types::PhysicalType::INT16:
                total += sizeof(int16_t);
                break;
            case Types::PhysicalType::INT32:
                total += sizeof(int32_t);
                break;
            case Types::PhysicalType::INT64:
                total += sizeof(int64_t);
                break;
            case Types::PhysicalType::INT128:
                total += sizeof(Int128);
                break;
            case Types::PhysicalType::STRING:
                return SIZE_MAX;
        }
    }
    return total;
}

uint64_t GroupByKeySerializer::PackInt64(const std::vector<ColumnSpan>& keyCols, size_t idx) {
    uint64_t key = 0;
    char* pos = reinterpret_cast<char*>(&key);
    for (const auto& col : keyCols) {
        std::visit(Types::overloaded{
                       [&](std::span<const uint8_t> span) {
                           std::memcpy(pos, &span[idx], sizeof(span[idx]));
                           pos += sizeof(span[idx]);
                       },
                       [&](std::span<const int16_t> span) {
                           std::memcpy(pos, &span[idx], sizeof(span[idx]));
                           pos += sizeof(span[idx]);
                       },
                       [&](std::span<const int32_t> span) {
                           std::memcpy(pos, &span[idx], sizeof(span[idx]));
                           pos += sizeof(span[idx]);
                       },
                       [&](std::span<const int64_t> span) {
                           std::memcpy(pos, &span[idx], sizeof(span[idx]));
                           pos += sizeof(span[idx]);
                       },
                       [&](const auto&) {},
                   },
                   col);
    }
    return key;
}

Int128 GroupByKeySerializer::PackInt128(const std::vector<ColumnSpan>& keyCols, size_t idx) {
    Int128 key = 0;
    char* pos = reinterpret_cast<char*>(&key);
    for (const auto& col : keyCols) {
        std::visit(Types::overloaded{
                       [&](std::span<const uint8_t> span) {
                           std::memcpy(pos, &span[idx], sizeof(span[idx]));
                           pos += sizeof(span[idx]);
                       },
                       [&](std::span<const int16_t> span) {
                           std::memcpy(pos, &span[idx], sizeof(span[idx]));
                           pos += sizeof(span[idx]);
                       },
                       [&](std::span<const int32_t> span) {
                           std::memcpy(pos, &span[idx], sizeof(span[idx]));
                           pos += sizeof(span[idx]);
                       },
                       [&](std::span<const int64_t> span) {
                           std::memcpy(pos, &span[idx], sizeof(span[idx]));
                           pos += sizeof(span[idx]);
                       },
                       [&](std::span<const Int128> span) {
                           std::memcpy(pos, &span[idx], sizeof(span[idx]));
                           pos += sizeof(span[idx]);
                       },
                       [](const auto&) {},
                   },
                   col);
    }
    return key;
}

InlineKey MakeInlineKey(const char* data, size_t len, KeysArena* arena) {
    InlineKey key;
    key.hash = std::hash<std::string_view>{}({data, len});
    key.len = static_cast<uint32_t>(len);

    const size_t prefixLen = std::min(len, InlineKey::kPrefixBytes);
    std::memcpy(key.prefix, data, prefixLen);

    if (len > InlineKey::kPrefixBytes) {
        key.arenaOffset = arena->Push(data, len);
    }
    return key;
}

size_t GroupByKeySerializer::Serialize(char* dest, const std::vector<ColumnSpan>& keyCols, size_t idx) {
    char* pos = dest;
    for (const auto& col : keyCols) {
        std::visit(Types::overloaded{
                       [&](std::span<const std::string> span) {
                           const uint32_t len = static_cast<uint32_t>(span[idx].size());
                           std::memcpy(pos, &len, sizeof(len));
                           pos += sizeof(len);
                           std::memcpy(pos, span[idx].data(), len);
                           pos += len;
                       },
                       [&](std::span<const Int128> span) {
                           std::memcpy(pos, &span[idx], sizeof(span[idx]));
                           pos += sizeof(span[idx]);
                       },
                       [&]<typename T>(std::span<const T> span) {
                           const int64_t v = static_cast<int64_t>(span[idx]);
                           std::memcpy(pos, &v, sizeof(v));
                           pos += sizeof(v);
                       },
                   },
                   col);
    }
    return static_cast<size_t>(pos - dest);
}

std::vector<Types::AnyPhysicalType> GroupByKeySerializer::DeserializeInline(std::string_view key, const std::vector<GroupByKey>& keyDefs) {
    const char* pos = key.data();
    std::vector<Types::AnyPhysicalType> values;
    values.reserve(keyDefs.size());

    for (const auto& keydef : keyDefs) {
        switch (Types::ToPhysical(keydef.expr->ResultType())) {
            case Types::PhysicalType::STRING: {
                uint32_t len;
                std::memcpy(&len, pos, sizeof(len));
                pos += sizeof(len);
                values.push_back(std::string{pos, len});
                pos += len;
                break;
            }
            case Types::PhysicalType::INT128: {
                Int128 v;
                std::memcpy(&v, pos, sizeof(v));
                pos += sizeof(v);
                values.push_back(v);
                break;
            }
            case Types::PhysicalType::INT16: {
                int64_t v;
                std::memcpy(&v, pos, sizeof(v));
                pos += sizeof(v);
                values.push_back(static_cast<int16_t>(v));
                break;
            }
            case Types::PhysicalType::INT32: {
                int64_t v;
                std::memcpy(&v, pos, sizeof(v));
                pos += sizeof(v);
                values.push_back(static_cast<int32_t>(v));
                break;
            }
            case Types::PhysicalType::INT64: {
                int64_t v;
                std::memcpy(&v, pos, sizeof(v));
                pos += sizeof(v);
                values.push_back(v);
                break;
            }
            case Types::PhysicalType::BOOL: {
                int64_t v;
                std::memcpy(&v, pos, sizeof(v));
                pos += sizeof(v);
                values.push_back(static_cast<uint8_t>(v));
                break;
            }
        }
    }
    return values;
}

std::vector<Types::AnyPhysicalType> GroupByKeySerializer::DeserializePacked(const void* keyData, const std::vector<GroupByKey>& keyDefs) {
    const char* pos = static_cast<const char*>(keyData);
    std::vector<Types::AnyPhysicalType> values;
    values.reserve(keyDefs.size());

    for (const auto& keydef : keyDefs) {
        switch (Types::ToPhysical(keydef.expr->ResultType())) {
            case Types::PhysicalType::BOOL: {
                uint8_t value;
                std::memcpy(&value, pos, sizeof(value));
                pos += sizeof(value);
                values.push_back(value);
                break;
            }
            case Types::PhysicalType::INT16: {
                int16_t value;
                std::memcpy(&value, pos, sizeof(value));
                pos += sizeof(value);
                values.push_back(value);
                break;
            }
            case Types::PhysicalType::INT32: {
                int32_t value;
                std::memcpy(&value, pos, sizeof(value));
                pos += sizeof(value);
                values.push_back(value);
                break;
            }
            case Types::PhysicalType::INT64: {
                int64_t value;
                std::memcpy(&value, pos, sizeof(value));
                pos += sizeof(value);
                values.push_back(value);
                break;
            }
            case Types::PhysicalType::INT128: {
                Int128 value;
                std::memcpy(&value, pos, sizeof(value));
                pos += sizeof(value);
                values.push_back(value);
                break;
            }
            case Types::PhysicalType::STRING:
                break;
        }
    }
    return values;
}

}  // namespace Columnar::Exec
