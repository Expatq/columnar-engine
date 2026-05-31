#pragma once

#include <exec/core/exec_batch.h>
#include <exec/core/selection_vector.h>
#include <exec/interface/expression.h>

#include <core/types.h>

#include <memory>
#include <string>

namespace re2 {
class RE2;
}  // namespace re2

namespace Columnar::Exec {

/*
3 layers of optimisation

L1: when patten doesn`t contain any of ".*+?^${}()|[\", use std::string::find + reserve + append
L2: extracts max literal prefix from pattern, pushdown strings that dont contain it
L3: use google re2
*/

class RegexReplaceExpression : public IExpression {
public:
    RegexReplaceExpression(std::unique_ptr<IExpression> input, const std::string& pattern, std::string replacement);
    ~RegexReplaceExpression() override;

    ExpressionKind Kind() const override;
    Types::LogicalType ResultType() const override;
    std::vector<std::string> RequiredColumns() const override;

    ColumnSpan EvaluateColumn(const ExecBatch& input, EvalState& state) const override;
    Types::AnyPhysicalType EvaluateScalar(const ExecBatch& input, RowId row) const override;

private:
    std::string ApplyReplace(const std::string& str) const;
    std::string TrivialReplace(const std::string& str) const;

private:
    std::unique_ptr<IExpression> input_;
    std::string replacement_;
    mutable EvalState inputState_;

    // L1: trivial types
    bool isTrivial_;
    std::string trivialPattern_;

    // L2: literal prefix pre-screening
    bool isAnchored_;
    std::string literalPrefix_;

    std::unique_ptr<re2::RE2> re_;
};

}  // namespace Columnar::Exec
