#pragma once

#include "inline_key.h"
#include "serializer.h"

#include <exec/aggregate/lib/spec.h>
#include <exec/aggregate/lib/state.h>

#include <exec/core/exec_batch.h>

#include <exec/interface/expression.h>
#include <exec/interface/operator.h>

#include <core/row_group.h>
#include <core/types.h>

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace Columnar::Exec {

class GroupByAggregation : public IOperator {
public:
    GroupByAggregation(std::unique_ptr<IOperator> child,
                       std::vector<GroupByKey> keys,
                       std::vector<AggregateSpec> aggregates);

    void Open() override;
    bool Next(ExecBatch& out) override;
    void Close() noexcept override;

private:
    struct GroupEntry {
        std::vector<AggregateState> states;
    };

    KeyMode CalcKeyMode();
    void Consume(const ExecBatch& batch);
    RowGroup BuildResult();
    std::vector<AggregateState> MakeInitialStates() const;

    std::unique_ptr<IOperator> child_;
    std::vector<GroupByKey> keys_;
    std::vector<AggregateSpec> aggregates_;

    KeyMode keyMode_;
    KeysArena arena_;

    std::unordered_map<uint64_t, GroupEntry> groupsInt64_;
    std::unordered_map<Int128, GroupEntry, Int128Hash> groupsInt128_;
    std::unordered_map<InlineKey, GroupEntry, InlineKeyHash, InlineKeyEq> groupsInline_{0, InlineKeyHash{}, InlineKeyEq{&arena_}};

    std::vector<EvalState> keyStates_;
    ExecBatch input_;
    bool produced_ = false;
};

}  // namespace Columnar::Exec
