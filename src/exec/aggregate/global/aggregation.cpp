#include "aggregation.h"

#include <exec/result_format/row_group_builder.h>

#include <exec/aggregate/lib/aggregate_result.h>
#include <exec/aggregate/lib/consumers.h>
#include <exec/aggregate/lib/spec.h>
#include <exec/aggregate/lib/state.h>
#include <exec/core/exec_batch.h>
#include <exec/core/selection_vector.h>
#include <exec/interface/expression.h>
#include <exec/interface/operator.h>

#include <core/row_group.h>
#include <core/schema.h>

#include <util/assert.h>
#include <util/int128.h>

namespace Columnar::Exec {

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
                case Types::PhysicalType::INT128:
                    return CountDistinctState<Int128>{};
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
                case Types::PhysicalType::INT64:
                case Types::PhysicalType::INT128:
                    return SumState<Int128>{};
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
                case Types::PhysicalType::INT128:
                    return MinMaxState<Int128>{};
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
                std::visit(Types::overloaded{
                               [&]<typename T>(CountDistinctState<T>& ds) { ConsumeCountDistinct(span, ds); },
                               [](auto&) {},
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
                std::visit(Types::overloaded{
                               [&](std::span<const std::string> s) {
                                   auto& mm = std::get<MinMaxState<std::string>>(state);
                                   const bool isMin = (spec.kind == AggregateKind::Min);
                                   if (!isMin && spec.kind != AggregateKind::Max)
                                       throw std::runtime_error("unsupported aggregate on STRING");
                                   for (const std::string& v : s)
                                       if (!mm.value.has_value() || (isMin ? v < *mm.value : v > *mm.value))
                                           mm.value = v;
                               },
                               [](std::span<const uint8_t>) {
                                   throw std::runtime_error("aggregate on BOOL not supported");
                               },
                               [&]<typename T>(std::span<const T> s) { ConsumeTyped<T, Int128>(s, spec.kind, state); },
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
        AppendAggregateResult(builder, i, states_[i]);
    }
    return builder.Finish();
}

}  // namespace Columnar::Exec
