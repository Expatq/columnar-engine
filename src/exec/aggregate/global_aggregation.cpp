#include "global_aggregation.h"

#include <exec/result_format/row_group_builder.h>

#include <exec/core/exec_batch.h>
#include <exec/core/selection_vector.h>
#include "core/row_group.h"
#include "core/schema.h"
#include "exec/aggregate/spec.h"
#include "exec/aggregate/consumers.h"
#include "exec/aggregate/state.h"
#include "exec/interface/expression.h"
#include "exec/interface/operator.h"
#include "util/assert.h"

namespace Columnar::Exec {

namespace {

void AppendResult(RowGroupBuilder& builder, size_t idx, const AggregateState& state) {
    std::visit(
        [&](const auto& s) {
            using S = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<S, CountState>) {
                builder.Append<int64_t>(idx, static_cast<int64_t>(s.count));
            } else if constexpr (std::is_same_v<S, SumState<int64_t>>) {
                builder.Append<int64_t>(idx, s.sum);
            } else if constexpr (std::is_same_v<S, SumState<double>>) {
                builder.Append<int64_t>(idx, static_cast<int64_t>(s.sum));
            } else if constexpr (std::is_same_v<S, AvgState>) {
                builder.Append<int64_t>(idx, s.Result());
            } else if constexpr (requires { s.seen.size(); }) {
                builder.Append<int64_t>(idx, static_cast<int64_t>(s.seen.size()));
            } else {
                // MinMaxState<T>
                using V = std::decay_t<decltype(*s.value)>;
                builder.Append<V>(idx, s.value.has_value() ? *s.value : V{});
            }
        },
        state);
}

}  // namespace

GlobalAggregation::GlobalAggregation(std::unique_ptr<IOperator> child, std::vector<AggregateSpec> aggregates)
    : child_(std::move(child)),
      aggregates_(std::move(aggregates)) {
    if (!child_) {
        throw std::invalid_argument("GlobalAggregation requires child operator");
    }
    if (aggregates_.empty()) {
        throw std::invalid_argument("GlobalAggregation requires at least one aggregate");
    }
}

void GlobalAggregation::Open() {
    child_->Open();
    states_.clear();
    states_.reserve(aggregates_.size());
    for (const auto& spec : aggregates_) {
        states_.push_back(MakeInitialState(spec));
    }
    inputStates_.resize(aggregates_.size());
    produced_ = false;
}

bool GlobalAggregation::Next(ExecBatch& out) {
    if (produced_) {
        return false;
    }

    while (child_->Next(input_)) {
        Consume(input_);
    }

    out.Reset();
    out.rowGroup.emplace(BuildResult());
    out.rowCount = 1;
    produced_ = true;
    return true;
}

void GlobalAggregation::Close() noexcept {
    child_->Close();
    input_.Reset();
    states_.clear();
    produced_ = false;
}

AggregateState GlobalAggregation::MakeInitialState(const AggregateSpec& spec) const {
    switch (spec.kind) {
        case AggregateKind::CountStar:
        case AggregateKind::CountColumn:
            return CountState{};
        case AggregateKind::CountDistinct:
            COLUMNAR_ASSERT(spec.HasInput(), "CountDistinct needs input");
            switch (Types::ToPhysical(spec.InputType())) {
                case Types::PhysicalType::INT16:
                case Types::PhysicalType::INT32:
                    return CountDistinctState<int32_t>{};
                case Types::PhysicalType::INT64:
                    return CountDistinctState<int64_t>{};
                case Types::PhysicalType::STRING:
                    return CountDistinctState<std::string>{};
                default:
                    throw std::runtime_error("CountDistinct: unsupported type");
            }
        case AggregateKind::Sum:
            COLUMNAR_ASSERT(spec.HasInput(), "Sum requires input expression");
            switch (Types::ToPhysical(spec.InputType())) {
                case Types::PhysicalType::INT16:
                case Types::PhysicalType::INT32:
                    return SumState<int64_t>{};
                case Types::PhysicalType::INT64:
                    return SumState<double>{};
                case Types::PhysicalType::BOOL:
                case Types::PhysicalType::STRING:
                    throw std::runtime_error("SUM not supported for BOOL/STRING");
            }
            break;
        case AggregateKind::Avg:
            return AvgState{};
        case AggregateKind::Min:
        case AggregateKind::Max: {
            COLUMNAR_ASSERT(spec.HasInput(), "Min/Max requires input expression");
            switch (Types::ToPhysical(spec.InputType())) {
                case Types::PhysicalType::INT16:
                    return MinMaxState<int16_t>{};
                case Types::PhysicalType::INT32:
                    return MinMaxState<int32_t>{};
                case Types::PhysicalType::INT64:
                    return MinMaxState<int64_t>{};
                case Types::PhysicalType::BOOL:
                    return MinMaxState<uint8_t>{};
                case Types::PhysicalType::STRING:
                    return MinMaxState<std::string>{};
            }
        }
    }
    throw std::runtime_error("unknown aggregate kind in MakeInitialState");
}

void GlobalAggregation::Consume(const ExecBatch& batch) {
    for (size_t i = 0; i < aggregates_.size(); ++i) {
        const auto& spec = aggregates_[i];
        auto& state = states_[i];

        switch (spec.kind) {
            case AggregateKind::CountStar:
                std::get<CountState>(state).count += batch.ActiveRowCount();
                break;
            case AggregateKind::CountColumn:
                std::get<CountState>(state).count += batch.ActiveRowCount();
                break;
            case AggregateKind::CountDistinct: {
                COLUMNAR_ASSERT(spec.HasInput(), "CountDistinct needs input");
                const ColumnSpan span = spec.input->EvaluateColumn(batch, inputStates_[i]);
                std::visit([&](auto& ds) {
                    if constexpr (requires { ds.seen.size(); }) {
                        ConsumeCountDistinct(span, ds);
                    }
                },
                           state);
                break;
            }

            case AggregateKind::Sum:
            case AggregateKind::Avg:
            case AggregateKind::Min:
            case AggregateKind::Max: {
                COLUMNAR_ASSERT(spec.HasInput(), "aggregate requires input");
                const ColumnSpan span =
                    spec.input->EvaluateColumn(batch, inputStates_[i]);
                std::visit([&](const auto& s) {
                    using T = std::remove_cv_t<typename std::decay_t<decltype(s)>::element_type>;
                    if constexpr (std::is_same_v<T, std::string>) {
                        // String: only Min/Max supported.
                        auto& mm = std::get<MinMaxState<std::string>>(state);
                        const bool isMin = (spec.kind == AggregateKind::Min);
                        if (!isMin && spec.kind != AggregateKind::Max)
                            throw std::runtime_error("unsupported aggregate on STRING");
                        for (const std::string& v : s)
                            if (!mm.value.has_value() || (isMin ? v < *mm.value : v > *mm.value)) {
                                mm.value = v;
                            }
                    } else if constexpr (std::is_same_v<T, uint8_t>) {
                        throw std::runtime_error("aggregate on BOOL not supported");
                    } else if constexpr (std::is_same_v<T, int64_t>) {
                        ConsumeTyped<T, double>(s, spec.kind, state);
                    } else {
                        ConsumeTyped<T, int64_t>(s, spec.kind, state);
                    }
                },
                           span);
                break;
            }
        }
    }
}

RowGroup GlobalAggregation::BuildResult() const {
    Schema schema;
    for (const auto& spec : aggregates_) {
        schema.AddColumn(spec.outputName, spec.outputType);
    }

    RowGroupBuilder builder(std::move(schema));
    for (size_t i = 0; i < aggregates_.size(); ++i) {
        AppendResult(builder, i, states_[i]);
    }
    return builder.Finish();
}

}  // namespace Columnar::Exec
