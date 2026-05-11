#pragma once

#include "state.h"
#include "spec.h"

#include <exec/interface/expression.h>
#include <core/types.h>

#include <span>
#include <stdexcept>

namespace Columnar::Exec {

template <typename T, typename SumAccumT>
void ConsumeTyped(std::span<const T> data, AggregateKind kind, AggregateState& state) {
    switch (kind) {
        case AggregateKind::Sum:
            for (const T& v : data)
                std::get<SumState<SumAccumT>>(state).sum += static_cast<SumAccumT>(v);
            return;
        case AggregateKind::Avg: {
            auto& a = std::get<AvgState>(state);
            for (const T& v : data) { a.sum += static_cast<Int128>(v); ++a.count; }
            return;
        }
        case AggregateKind::Min: {
            auto& mm = std::get<MinMaxState<T>>(state);
            for (const T& v : data)
                if (!mm.value.has_value() || v < *mm.value) mm.value = v;
            return;
        }
        case AggregateKind::Max: {
            auto& mm = std::get<MinMaxState<T>>(state);
            for (const T& v : data)
                if (!mm.value.has_value() || v > *mm.value) mm.value = v;
            return;
        }
        default:
            throw std::runtime_error("unsupported aggregate kind for this column type");
    }
}

template <typename StateT>
void ConsumeCountDistinct(const ColumnSpan& span, StateT& state) {
    using Key = typename StateT::value_type;
    std::visit(Types::overloaded{
        [&]<typename T>(std::span<const T> s) requires std::is_convertible_v<T, Key> {
            for (const T& v : s) state.seen.insert(static_cast<Key>(v));
        },
        [](const auto&) {},
    }, span);
}

inline void InsertDistinct(const Types::AnyPhysicalType& val, AggregateState& state) {
    std::visit([&](const auto& v) {
        using V = std::decay_t<decltype(v)>;
        std::visit(Types::overloaded{
            [&]<typename T>(CountDistinctState<T>& ds) requires std::is_convertible_v<V, T> {
                ds.seen.insert(static_cast<T>(v));
            },
            [](auto&) {},
        }, state);
    }, val);
}

}  // namespace Columnar::Exec
