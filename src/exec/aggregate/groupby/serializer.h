#pragma once

#include "inline_key.h"

#include <exec/interface/expression.h>

#include <absl/types/span.h>

#include <string>
#include <string_view>

namespace Columnar::Exec {

struct GroupByKey {
    std::unique_ptr<IExpression> expr;
    std::string outputName;
};

struct GroupByKeySerializer {
    static constexpr size_t kMaxKeyBytes = 512;

    static size_t PackedSize(absl::Span<const GroupByKey> keyDefs);

    static uint64_t PackInt64(const std::vector<ColumnSpan>& keyCols, size_t idx);
    static Int128 PackInt128(const std::vector<ColumnSpan>& keyCols, size_t idx);

    static InlineKey MakeInlineKey(const char* data, size_t len, KeysArena* arena);

    /*
    Serializes cell at 'idx' in each of key columns and stores it in dest
    Returns total bytes written
    */
    static std::string Serialize(const std::vector<ColumnSpan>& keyCols, size_t idx);

    static std::vector<Types::AnyPhysicalType> DeserializeInline(std::string_view key, absl::Span<const GroupByKey> keyDefs);
    static std::vector<Types::AnyPhysicalType> DeserializePacked(const void* keyData, absl::Span<const GroupByKey> keyDefs);
};

}  // namespace Columnar::Exec
