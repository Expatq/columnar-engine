#pragma once

#include <core/types.h>
#include <optional>
#include <variant>

namespace NColumnar {

struct TStatistics {
    using TMinMax = std::variant<std::monostate, int16_t, int32_t, int64_t,
                                 bool, std::string>;
    
    // TODO: use enum below for predicate pushdown
    enum class ECompareOp {
        Equal,
        NotEqual,
        Less,
        LessOrEqual,
        Greater,
        GreaterOrEqual
    };

    size_t rowCount = 0;
    size_t nullCount = 0;

    TMinMax min_value;
    TMinMax max_value;

    std::optional<size_t> minStringLength;
    std::optional<size_t> maxStringLength;
    std::optional<size_t> totalStringLength;

    std::optional<size_t> uniqueCount;

public:
    TStatistics() = default;

    explicit TStatistics(NTypes::EDataType type);

    // TODO: implement statistics methods
};

}  // namespace NColumnar