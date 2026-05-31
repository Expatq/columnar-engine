#include "regex_replace.h"

#include <exec/core/exec_batch.h>

#include <re2/re2.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include "core/types.h"
#include "exec/interface/expression.h"

namespace Columnar::Exec {

namespace {

bool IsTrivial(const std::string& pattern) {
    return pattern.find_first_of(R"(.*+?^${}()|[\)") == std::string::npos;
}

std::string TranslateReplacement(std::string_view src) {
    std::string out;
    out.reserve(src.size());
    for (size_t i = 0; i < src.size(); ++i) {
        const char ch = src[i];
        if (ch == '$' && i + 1 < src.size() && src[i + 1] >= '0' && src[i + 1] <= '9') {
            out += '\\';
            out += src[i + 1];
            ++i;
            continue;
        }
        out += ch;
    }
    return out;
}

std::string ExtractLiteralPrefix(const std::string& pattern, bool isAnchored) {
    constexpr std::string_view kSpecials = R"(.*+?${}()|[)";
    constexpr std::string_view kQuantifiers = "?*{";

    const size_t startPos = isAnchored ? 1 : 0;
    std::string result;
    result.reserve(pattern.size());

    bool isEscaped = false;
    for (size_t i = startPos; i < pattern.size(); ++i) {
        const char ch = pattern[i];

        if (isEscaped) {
            if (i + 1 < pattern.size() && kQuantifiers.find(pattern[i + 1]) != std::string_view::npos)
                break;
            result += ch;
            isEscaped = false;
            continue;
        }
        if (ch == '\\') {
            isEscaped = true;
            continue;
        }
        if (kSpecials.find(ch) != std::string_view::npos)
            break;
        if (i + 1 < pattern.size() && kQuantifiers.find(pattern[i + 1]) != std::string_view::npos)
            break;
        result += ch;
    }
    return result;
}

}  // namespace

RegexReplaceExpression::RegexReplaceExpression(std::unique_ptr<IExpression> input, const std::string& pattern, std::string replacement)
    : input_(std::move(input)),
      replacement_(std::move(replacement)),
      isTrivial_(IsTrivial(pattern)),
      isAnchored_(!pattern.empty() && pattern[0] == '^') {
    if (!input_) {
        throw std::invalid_argument("input cannot be null");
    }
    if (input_->ResultType() != Types::LogicalType::STRING) {
        throw std::invalid_argument("requires STRING input");
    }

    if (isTrivial_) {
        trivialPattern_ = pattern;
        return;
    }

    re2::RE2::Options opts;
    opts.set_log_errors(false);
    opts.set_case_sensitive(true);

    re_ = std::make_unique<re2::RE2>(pattern, opts);
    if (!re_->ok()) {
        throw std::invalid_argument("invalid pattern: " + re_->error());
    }

    replacement_ = TranslateReplacement(replacement_);
    literalPrefix_ = ExtractLiteralPrefix(pattern, isAnchored_);
}

RegexReplaceExpression::~RegexReplaceExpression() = default;

ExpressionKind RegexReplaceExpression::Kind() const {
    return ExpressionKind::RegexReplace;
}

Types::LogicalType RegexReplaceExpression::ResultType() const {
    return Types::LogicalType::STRING;
}

std::vector<std::string> RegexReplaceExpression::RequiredColumns() const {
    return input_->RequiredColumns();
}

ColumnSpan RegexReplaceExpression::EvaluateColumn(const ExecBatch& input, EvalState& state) const {
    const ColumnSpan col = input_->EvaluateColumn(input, inputState_);
    const auto& strs = std::get<std::span<const std::string>>(col);

    auto out = state.ResizeBuffer<std::string>(strs.size());
    for (size_t i = 0; i < strs.size(); ++i) {
        out[i] = ApplyReplace(strs[i]);
    }
    return std::span<const std::string>{out.data(), strs.size()};
}

Types::AnyPhysicalType RegexReplaceExpression::EvaluateScalar(const ExecBatch& input, RowId row) const {
    const Types::AnyPhysicalType scalar = input_->EvaluateScalar(input, row);
    return ApplyReplace(std::get<std::string>(scalar));
}

std::string RegexReplaceExpression::ApplyReplace(const std::string& str) const {
    if (isTrivial_) {
        return TrivialReplace(str);
    }

    if (!literalPrefix_.empty()) {
        if (isAnchored_) {
            if (str.size() < literalPrefix_.size() || str.compare(0, literalPrefix_.size(), literalPrefix_) != 0) {
                return str;
            }
        } else {
            if (str.find(literalPrefix_) == std::string::npos) {
                return str;
            }
        }
    }

    std::string result = str;
    if (re2::RE2::GlobalReplace(&result, *re_, replacement_) == 0) {
        return str;
    }
    return result;
}

std::string RegexReplaceExpression::TrivialReplace(const std::string& str) const {
    const size_t patternLen = trivialPattern_.size();
    if (patternLen == 0) {
        return str;
    }

    std::string result;
    result.reserve(str.size());

    size_t pos = 0;
    while (true) {
        const size_t found = str.find(trivialPattern_, pos);
        if (found == std::string::npos) {
            result.append(str, pos);
            break;
        }
        result.append(str, pos, found - pos);
        result.append(replacement_);
        pos = found + patternLen;
    }
    return result;
}

}  // namespace Columnar::Exec
