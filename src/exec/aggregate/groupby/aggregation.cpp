#include "aggregation.h"
#include "inline_key.h"
#include "serializer.h"

#include <exec/aggregate/lib/aggregate_result.h>
#include <exec/aggregate/lib/consumers.h>
#include <exec/aggregate/lib/spec.h>

#include <exec/result_format/row_group_builder.h>
#include <exec/interface/operator.h>
#include <exec/core/exec_batch.h>

#include <core/types.h>

#include <util/assert.h>

namespace Columnar::Exec {

namespace {

void UpdateAggState(const ExecBatch& batch, RowId row, const AggregateSpec& spec, AggregateState& state) {
    switch (spec.kind) {
        case AggregateKind::CountStar:
        case AggregateKind::CountColumn:
            std::get<CountState>(state).count += 1;
            return;

        case AggregateKind::CountDistinct:
            COLUMNAR_ASSERT(spec.HasInput(), "CountDistinct needs input");
            InsertDistinct(spec.input->EvaluateScalar(batch, row), state);
            return;
        case AggregateKind::Sum:
            std::visit([&](const auto& v) {
                using V = std::decay_t<decltype(v)>;
                if constexpr (std::is_integral_v<V> && !std::is_same_v<V, uint8_t>)
                    std::get<SumState<Int128>>(state).sum += static_cast<Int128>(v);
            },
                       spec.input->EvaluateScalar(batch, row));
            return;

        case AggregateKind::Avg:
            std::visit([&](const auto& v) {
                using V = std::decay_t<decltype(v)>;
                if constexpr (std::is_integral_v<V> && !std::is_same_v<V, uint8_t>) {
                    auto& avg = std::get<AvgState>(state);
                    avg.sum += static_cast<Int128>(v);
                    ++avg.count;
                }
            },
                       spec.input->EvaluateScalar(batch, row));
            return;

        case AggregateKind::Min:
        case AggregateKind::Max:
            std::visit([&](const auto& val) {
                using V = std::decay_t<decltype(val)>;
                std::visit([&](auto& mm) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(mm)>, MinMaxState<V>>) {
                        const bool better = spec.kind == AggregateKind::Min
                                                ? (!mm.value.has_value() || val < *mm.value)
                                                : (!mm.value.has_value() || val > *mm.value);
                        if (better)
                            mm.value = val;
                    }
                },
                           state);
            },
                       spec.input->EvaluateScalar(batch, row));
            return;
    }
}

}  // namespace

GroupByAggregation::GroupByAggregation(std::unique_ptr<IOperator> child, std::vector<GroupByKey> keys, std::vector<AggregateSpec> aggregates)
    : child_(std::move(child)),
      keys_(std::make_move_iterator(keys.begin()), std::make_move_iterator(keys.end())),
      aggregates_(std::make_move_iterator(aggregates.begin()), std::make_move_iterator(aggregates.end())),
      groupsInline_(0, InlineKeyHash{}, InlineKeyEq{&arena_}),
      produced_(false) {
    if (!child_ || keys_.empty() || aggregates_.empty())
        throw std::invalid_argument("GroupByAggregation: invalid arguments");
}

void GroupByAggregation::Open() {
    child_->Open();

    keyMode_ = CalcKeyMode();
    groupsInt64_.clear();
    groupsInt128_.clear();
    groupsInline_.clear();
    arena_.Reset();
    produced_ = false;
    keyStates_.resize(keys_.size());
}

bool GroupByAggregation::Next(ExecBatch& out) {
    if (produced_) {
        return false;
    }
    while (child_->Next(input_)) {
        Consume(input_);
    }
    out.Reset();
    out.rowGroup = std::make_shared<RowGroup>(BuildResult());
    out.rowCount = out.rowGroup->GetRowCount();
    produced_ = true;
    return true;
}

void GroupByAggregation::Close() noexcept {
    child_->Close();
    groupsInt64_.clear();
    groupsInt128_.clear();
    groupsInline_.clear();
    arena_.Reset();
    input_.Reset();
    produced_ = false;
}

KeyMode GroupByAggregation::CalcKeyMode() {
    const size_t keysPackedSize = GroupByKeySerializer::PackedSize(keys_);
    return keysPackedSize <= sizeof(uint64_t) ? KeyMode::Int64
           : keysPackedSize <= sizeof(Int128) ? KeyMode::Int128
                                              : KeyMode::Inline;
}

void GroupByAggregation::Consume(const ExecBatch& batch) {
    std::vector<ColumnSpan> keyCols;
    keyCols.reserve(keys_.size());
    for (size_t i = 0; i < keys_.size(); ++i) {
        keyCols.push_back(keys_[i].expr->EvaluateColumn(batch, keyStates_[i]));
    }

    const size_t activeCount = batch.has_selection ? batch.selection.Size() : batch.rowCount;

    alignas(8) char keyBuf[GroupByKeySerializer::kMaxKeyBytes];
    for (size_t idx = 0; idx < activeCount; ++idx) {
        const RowId physRow = batch.has_selection ? batch.selection.Rows()[idx] : idx;

        GroupEntry* entry = nullptr;
        switch (keyMode_) {
            case KeyMode::Int64: {
                const uint64_t key = GroupByKeySerializer::PackInt64(keyCols, idx);
                auto it = groupsInt64_.find(key);
                if (it == groupsInt64_.end()) {
                    it = groupsInt64_.emplace(key, GroupEntry{MakeInitialStates()}).first;
                }
                entry = &it->second;
                break;
            }
            case KeyMode::Int128: {
                const Int128 key = GroupByKeySerializer::PackInt128(keyCols, idx);
                auto it = groupsInt128_.find(key);
                if (it == groupsInt128_.end()) {
                    it = groupsInt128_.emplace(key, GroupEntry{MakeInitialStates()}).first;
                }
                entry = &it->second;
                break;
            }
            case KeyMode::Inline: {
                const size_t keyLen = GroupByKeySerializer::Serialize(keyBuf, keyCols, idx);
                const uint32_t savedPos = arena_.pos;
                const InlineKey key = GroupByKeySerializer::MakeInlineKey(keyBuf, keyLen, &arena_);

                auto it = groupsInline_.find(key);
                if (it == groupsInline_.end()) {
                    it = groupsInline_.emplace(key, GroupEntry{MakeInitialStates()}).first;
                } else if (keyLen > InlineKey::kPrefixBytes) {
                    arena_.pos = savedPos;
                }
                entry = &it->second;
                break;
            }
        }

        for (size_t i = 0; i < aggregates_.size(); ++i) {
            UpdateAggState(batch, physRow, aggregates_[i], entry->states[i]);
        }
    }
}

RowGroup GroupByAggregation::BuildResult() {
    Schema schema;
    for (const auto& k : keys_)
        schema.AddColumn(k.outputName, k.expr->ResultType());
    for (const auto& spec : aggregates_)
        schema.AddColumn(spec.outputName, spec.outputType);

    RowGroupBuilder builder(std::move(schema));

    auto appendRow = [&](const std::vector<Types::AnyPhysicalType>& keyVals, GroupEntry& entry) {
        for (size_t ki = 0; ki < keys_.size(); ++ki)
            std::visit([&](const auto& val) {
                builder.Append<std::decay_t<decltype(val)>>(ki, val);
            },
                       keyVals[ki]);
        const size_t aggOffset = keys_.size();
        for (size_t ai = 0; ai < aggregates_.size(); ++ai)
            AppendAggregateResult(builder, aggOffset + ai, entry.states[ai]);
    };

    switch (keyMode_) {
        case KeyMode::Int64:
            for (auto& [key, entry] : groupsInt64_)
            appendRow(GroupByKeySerializer::DeserializePacked(&key, keys_), entry);
            break;
        case KeyMode::Int128:
            for (auto& [key, entry] : groupsInt128_)
                appendRow(GroupByKeySerializer::DeserializePacked(&key, keys_), entry);
            break;
        case KeyMode::Inline:
            for (auto& [key, entry] : groupsInline_) {
                const std::string_view keyView{
                    key.IsInline() ? key.prefix : arena_.Data() + key.arenaOffset,
                    key.len};
                appendRow(GroupByKeySerializer::DeserializeInline(keyView, keys_), entry);
            }
            break;
    }
    return builder.Finish();
}

std::vector<AggregateState> GroupByAggregation::MakeInitialStates() const {
    std::vector<AggregateState> states;
    states.reserve(aggregates_.size());
    for (const auto& spec : aggregates_) {
        switch (spec.kind) {
            case AggregateKind::CountStar:
            case AggregateKind::CountColumn:
                states.push_back(CountState{});
                break;
            case AggregateKind::CountDistinct:
                COLUMNAR_ASSERT(spec.HasInput(), "CountDistinct needs input");
                switch (Types::ToPhysical(spec.InputType())) {
                    case Types::PhysicalType::INT16:
                    case Types::PhysicalType::INT32:
                        states.push_back(CountDistinctState<int32_t>{});
                        break;
                    case Types::PhysicalType::INT64:
                        states.push_back(CountDistinctState<int64_t>{});
                        break;
                    case Types::PhysicalType::INT128:
                        states.push_back(CountDistinctState<Int128>{});
                        break;
                    case Types::PhysicalType::STRING:
                        states.push_back(CountDistinctState<std::string>{});
                        break;
                    default:
                        throw std::runtime_error("CountDistinct: unsupported type");
                }
                break;
            case AggregateKind::Sum:
                states.push_back(SumState<Int128>{});
                break;
            case AggregateKind::Avg:
                states.push_back(AvgState{});
                break;
            case AggregateKind::Min:
            case AggregateKind::Max:
                COLUMNAR_ASSERT(spec.HasInput(), "Min/Max needs input");
                switch (Types::ToPhysical(spec.InputType())) {
                    case Types::PhysicalType::INT16:
                        states.push_back(MinMaxState<int16_t>{});
                        break;
                    case Types::PhysicalType::INT32:
                        states.push_back(MinMaxState<int32_t>{});
                        break;
                    case Types::PhysicalType::INT64:
                        states.push_back(MinMaxState<int64_t>{});
                        break;
                    case Types::PhysicalType::INT128:
                        states.push_back(MinMaxState<Int128>{});
                        break;
                    case Types::PhysicalType::BOOL:
                        states.push_back(MinMaxState<uint8_t>{});
                        break;
                    case Types::PhysicalType::STRING:
                        states.push_back(MinMaxState<std::string>{});
                        break;
                }
                break;
        }
    }
    return states;
}

}  // namespace Columnar::Exec
