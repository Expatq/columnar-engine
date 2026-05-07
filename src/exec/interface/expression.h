#pragma once

#include <core/types.h>
#include <exec/core/exec_batch.h>
#include <exec/core/selection_vector.h>

#include <util/assert.h>

#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace Columnar::Exec {

enum class ExpressionKind {
    ColumnRef,
    Literal,
    Comparison,
    Logical,
    Not,
    Arithmetic,
    Like,
    DateTrunc,
    StringLen,
    RegexReplace,
};

enum class CompareOp {
    Eq,
    NotEq,
    Lt,
    Lte,
    Gt,
    Gte
};

enum class LogicalOp {
    And,
    Or
};

enum class ArithmOp {
    Add,
    Sub,
    Mul,
    Div
};

enum class DateTruncUnit {
    Minute,
    Hour,
    Day,
    Month,
    Year
};

using ColumnSpan = std::variant<std::span<const int16_t>,
                                std::span<const int32_t>,
                                std::span<const int64_t>,
                                std::span<const Int128>,
                                std::span<const uint8_t>,
                                std::span<const std::string>>;

/*
Per expression output buffer
*/
struct EvalState {
    Types::AnyColumnData buffer;

    template <typename T>
    std::span<T> ResizeBuffer(size_t count) {
        if (!std::holds_alternative<std::vector<T>>(buffer)) {
            buffer = std::vector<T>{};
        }
        auto& vec = std::get<std::vector<T>>(buffer);
        vec.resize(count);
        return {vec.data(), count};
    }
};

class IExpression {
public:
    virtual ~IExpression() = default;

    virtual ExpressionKind Kind() const {
        throw std::runtime_error("expression has no kind");
    }

    virtual Types::LogicalType ResultType() const = 0;
    virtual std::vector<std::string> RequiredColumns() const = 0;

    /* Mode-1 - Filter: writes matching row ids into selection vector
    Evaluates to selection vector (output) using ExecBatch data (input)
    Implemented by: Comparison, Logical, Not, Like
    */
    virtual void EvaluateSelection(const ExecBatch& /*input*/, SelectionVector& /*out*/) const {
        COLUMNAR_ASSERT(false, "expression cannot produce selection");
    }

    /* Mode 2 — Columnar: returns non-owning view of active-row values.
       ColumnRef, no selection → zero-copy span into RowGroup's own storage.
       ColumnRef, with selection → span into state.buffer (filtered, contiguous).
       Arithmetic / DateTrunc → computed into state.buffer, span into it.
       state is owned by caller and reused across batches (no re-alloc after first).
    */
    virtual ColumnSpan EvaluateColumn(const ExecBatch& /*input*/, EvalState& /*state*/) const {
        COLUMNAR_ASSERT(false, "expression cannot produce column");
    }

    /* Mode-3 - Scalar: evaluates one row (slow per-row vtable call)
    Only used for CountDistinct
    */
    virtual Types::AnyPhysicalType EvaluateScalar(const ExecBatch& /*input*/, RowId /*row*/) const {
        COLUMNAR_ASSERT(false, "expression cannot produce scalar");
    }
};

}  // namespace Columnar::Exec
