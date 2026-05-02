#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <variant>

namespace Columnar::Exec {

struct CountState {
    uint64_t count = 0;
};

template <typename AccumT>
struct SumState {
    AccumT sum = 0;
};

struct AvgState {
    double sum = 0.0;
    uint64_t count = 0;

    int64_t Result() const {
        return count == 0 ? 0LL : static_cast<int64_t>(sum / static_cast<double>(count));
    }
};

template <typename T>
struct MinMaxState {
    std::optional<T> value;
};

template <typename T>
struct CountDistinctState {
    using value_type = T;
    std::unordered_set<T> seen;
};

using AggregateState = std::variant<
    CountState,
    CountDistinctState<int32_t>,
    CountDistinctState<int64_t>,
    CountDistinctState<std::string>,
    SumState<int64_t>,  // INT16 / INT32 input — exact
    SumState<double>,   // INT64 input — prevents accumulator overflow
    AvgState,
    MinMaxState<int16_t>,
    MinMaxState<int32_t>,
    MinMaxState<int64_t>,
    MinMaxState<uint8_t>,
    MinMaxState<std::string>>;

}  // namespace Columnar::Exec
