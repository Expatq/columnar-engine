#pragma once

#include "state.h"

#include <exec/result_format/row_group_builder.h>

#include <core/types.h>

namespace Columnar::Exec {

inline void AppendAggregateResult(RowGroupBuilder& builder,
                                   size_t colIdx,
                                   const AggregateState& state) {
    std::visit(Types::overloaded{
        [&](const CountState& s) {
            builder.Append<int64_t>(colIdx, static_cast<int64_t>(s.count));
        },
        [&](const SumState<Int128>& s) {
            builder.Append<int64_t>(colIdx, static_cast<int64_t>(s.sum));
        },
        [&](const AvgState& s) {
            builder.Append<int64_t>(colIdx, s.Result());
        },
        [&]<typename T>(const CountDistinctState<T>& s) {
            builder.Append<int64_t>(colIdx, static_cast<int64_t>(s.seen.size()));
        },
        [&]<typename T>(const MinMaxState<T>& s) {
            builder.Append<T>(colIdx, s.value.value_or(T{}));
        },
    }, state);
}

}  // namespace Columnar::Exec
