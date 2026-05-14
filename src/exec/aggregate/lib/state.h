#pragma once

#include <util/int128.h>

#include <cstdint>
#include <optional>
#include <string>
#include <absl/container/flat_hash_set.h>
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
    Int128 sum = 0;
    uint64_t count = 0;

    int64_t Result() const {
        return count == 0 ? 0LL : static_cast<int64_t>(static_cast<double>(sum) / static_cast<double>(count));
    }
};

template <typename T>
struct MinMaxState {
    std::optional<T> value;
};

template <typename T>
struct CountDistinctState {
    using value_type = T;
    absl::flat_hash_set<T> seen;
};

using AggregateState = std::variant<
    CountState,
    CountDistinctState<int32_t>,
    CountDistinctState<int64_t>,
    CountDistinctState<std::string>,
    CountDistinctState<Int128>,
    SumState<Int128>,
    AvgState,
    MinMaxState<int16_t>,
    MinMaxState<int32_t>,
    MinMaxState<int64_t>,
    MinMaxState<Int128>,
    MinMaxState<uint8_t>,
    MinMaxState<std::string>>;

}  // namespace Columnar::Exec
