#include <exec/expression/regex_replace/regex_replace.h>

#include <exec/expression/column_ref/column_ref.h>

#include <exec/core/exec_batch.h>
#include <exec/interface/expression.h>

#include <tests/lib/data_builders.h>
#include <tests/lib/exec_batch_builders.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Columnar::Test {

namespace {

constexpr auto kI64 = Types::LogicalType::INT64;
constexpr auto kStr = Types::LogicalType::STRING;

std::unique_ptr<Exec::IExpression> ColStr(std::string name) {
    return std::make_unique<Exec::ColumnRefExpression>(std::move(name), kStr);
}

std::unique_ptr<Exec::RegexReplaceExpression> Rx(const std::string& pattern,
                                                 const std::string& replacement) {
    return std::make_unique<Exec::RegexReplaceExpression>(ColStr("s"), pattern, replacement);
}

Exec::ExecBatch MakeStringBatch(std::vector<std::string> values) {
    return MakeBatch(MakeRowGroupOf(
        MakeSchema({{"s", kStr}}),
        MakeColumn<std::string>(std::move(values))));
}

std::vector<std::string> ApplyAll(const std::vector<std::string>& values,
                                  const std::string& pattern,
                                  const std::string& replacement) {
    Exec::EvalState state;
    auto batch = MakeStringBatch(values);
    auto expr = Rx(pattern, replacement);
    const auto columnSpan = expr->EvaluateColumn(batch, state);
    const auto& span = std::get<std::span<const std::string>>(columnSpan);
    return {span.begin(), span.end()};
}

class FixtureRegexReplace : public ::testing::Test {
protected:
    Exec::EvalState state_;
};

}  // namespace

TEST_F(FixtureRegexReplace, KindReportsRegexReplace) {
    auto expr = Rx("foo", "bar");

    EXPECT_EQ(expr->Kind(), Exec::ExpressionKind::RegexReplace);
}

TEST_F(FixtureRegexReplace, ResultTypeIsString) {
    auto expr = Rx("foo", "bar");

    EXPECT_EQ(expr->ResultType(), kStr);
}

TEST_F(FixtureRegexReplace, RequiredColumnsForwardsFromInput) {
    auto expr = std::make_unique<Exec::RegexReplaceExpression>(
        ColStr("referer"), "^https?://", "");

    const auto cols = expr->RequiredColumns();

    ASSERT_EQ(cols.size(), 1u);
    EXPECT_EQ(cols[0], "referer");
}

TEST_F(FixtureRegexReplace, ConstructorRejectsNullInput) {
    EXPECT_THROW(
        Exec::RegexReplaceExpression(nullptr, "foo", "bar"),
        std::invalid_argument);
}

TEST_F(FixtureRegexReplace, ConstructorRejectsNonStringInput) {
    EXPECT_THROW(
        Exec::RegexReplaceExpression(
            std::make_unique<Exec::ColumnRefExpression>("x", kI64),
            "foo", "bar"),
        std::invalid_argument);
}

TEST_F(FixtureRegexReplace, ConstructorRejectsInvalidPattern) {
    EXPECT_THROW(Rx("(unclosed", "x"), std::invalid_argument);
}

TEST_F(FixtureRegexReplace, TrivialPathReplacesAllOccurrencesNonOverlapping) {
    EXPECT_EQ(ApplyAll({"foo bar foo baz"}, "foo", "XX"),
              (std::vector<std::string>{"XX bar XX baz"}));
}

TEST_F(FixtureRegexReplace, TrivialPathOverlappingPatternConsumesAdvancePosition) {
    EXPECT_EQ(ApplyAll({"aaaa"}, "aa", "b"),
              (std::vector<std::string>{"bb"}))
        << "non-overlapping replace: 'aaaa' → 'aa'+'aa' → 'bb', not 'bbb' from overlapping";
}

TEST_F(FixtureRegexReplace, TrivialPathEmptyReplacementStripsMatches) {
    EXPECT_EQ(ApplyAll({"foobarfoobaz"}, "foo", ""),
              (std::vector<std::string>{"barbaz"}));
}

TEST_F(FixtureRegexReplace, TrivialPathNoMatchReturnsOriginal) {
    EXPECT_EQ(ApplyAll({"hello world"}, "xyz", "ABC"),
              (std::vector<std::string>{"hello world"}));
}

TEST_F(FixtureRegexReplace, TrivialPathEmptyPatternReturnsOriginal) {
    EXPECT_EQ(ApplyAll({"hello"}, "", "X"),
              (std::vector<std::string>{"hello"}));
}

TEST_F(FixtureRegexReplace, TrivialPathEmptyInputReturnsEmpty) {
    EXPECT_EQ(ApplyAll({""}, "foo", "bar"),
              (std::vector<std::string>{""}));
}

TEST_F(FixtureRegexReplace, TrivialPathReplacesAtBoundaries) {
    EXPECT_EQ(ApplyAll({"foofoofoo"}, "foo", "X"),
              (std::vector<std::string>{"XXX"}));
}

TEST_F(FixtureRegexReplace, RegexAnchoredPatternMatchesOnlyAtStart) {
    EXPECT_EQ(ApplyAll({"foo bar", "x foo"}, "^foo", "X"),
              (std::vector<std::string>{"X bar", "x foo"}))
        << "second row's 'foo' is not at start, so the L2 anchored prefix screen returns original";
}

TEST_F(FixtureRegexReplace, RegexDollarSignAnchoredAtEnd) {
    EXPECT_EQ(ApplyAll({"abc", "abcd"}, "abc$", "X"),
              (std::vector<std::string>{"X", "abcd"}));
}

TEST_F(FixtureRegexReplace, RegexOptionalCharMatchesBothHttpAndHttps) {
    EXPECT_EQ(ApplyAll(
                  {"http://example.com/", "https://example.com/", "ftp://example.com/"},
                  R"(^https?://)",
                  "//"),
              (std::vector<std::string>{"//example.com/", "//example.com/", "ftp://example.com/"}))
        << "literal-prefix screen must not over-constrain to 'https' and drop 'http://' matches";
}

TEST_F(FixtureRegexReplace, RegexCaptureGroupExtractsDomain) {
    EXPECT_EQ(ApplyAll(
                  {"http://example.com/path",
                   "https://www.example.com/foo/bar",
                   "no-match-here"},
                  R"(^https?://(?:www\.)?([^/]+)/.*$)",
                  "$1"),
              (std::vector<std::string>{"example.com", "example.com", "no-match-here"}))
        << "Q28-style domain extraction; no-match row keeps original (ClickHouse semantics)";
}

TEST_F(FixtureRegexReplace, RegexBackslashEscapeMatchesLiteralDot) {
    EXPECT_EQ(ApplyAll(
                  {"file.txt", "fileXtxt"},
                  R"(\.txt$)",
                  ".bak"),
              (std::vector<std::string>{"file.bak", "fileXtxt"}));
}

TEST_F(FixtureRegexReplace, RegexAlternationReplacesAnyBranch) {
    EXPECT_EQ(ApplyAll({"cat dog bird fish"}, "cat|dog", "X"),
              (std::vector<std::string>{"X X bird fish"}));
}

TEST_F(FixtureRegexReplace, RegexQuantifierPlusGreedy) {
    EXPECT_EQ(ApplyAll({"aaabbb"}, "a+", "X"),
              (std::vector<std::string>{"Xbbb"}));
}

TEST_F(FixtureRegexReplace, RegexCharClassNegated) {
    EXPECT_EQ(ApplyAll({"abc123def"}, "[^0-9]+", "_"),
              (std::vector<std::string>{"_123_"}));
}

TEST_F(FixtureRegexReplace, RegexIsCaseSensitiveByDefault) {
    EXPECT_EQ(ApplyAll({"Foo foo FOO"}, "foo", "X"),
              (std::vector<std::string>{"Foo X FOO"}));
}

TEST_F(FixtureRegexReplace, RegexUtf8InputAndPatternAreByteAccurate) {
    EXPECT_EQ(ApplyAll({"привет мир"}, "мир", "world"),
              (std::vector<std::string>{"привет world"}));
}

TEST_F(FixtureRegexReplace, ReplacementWithDollarSignBackrefSubstitutesGroups) {
    EXPECT_EQ(ApplyAll({"John Smith", "Jane Doe"}, R"(([A-Z][a-z]+) ([A-Z][a-z]+))", "$2 $1"),
              (std::vector<std::string>{"Smith John", "Doe Jane"}));
}

TEST_F(FixtureRegexReplace, ReplacementOnlyDigitsAfterDollarAreCaptureRefs) {
    EXPECT_EQ(ApplyAll({"abc"}, "(a)(b)(c)", "$2$$$3$x$1"),
              (std::vector<std::string>{"b$$c$xa"}))
        << "$N → \\N (capture), '$$' and '$x' stay literal";
}

TEST_F(FixtureRegexReplace, NoMatchOnAnchoredPatternReturnsOriginal) {
    EXPECT_EQ(ApplyAll({"xfoo", "yfoo"}, "^foo$", "X"),
              (std::vector<std::string>{"xfoo", "yfoo"}));
}

TEST_F(FixtureRegexReplace, EvaluateScalarRunsReplacementOnSingleRow) {
    auto batch = MakeStringBatch({"foo", "bar"});
    auto expr = Rx("foo", "X");

    EXPECT_EQ(std::get<std::string>(expr->EvaluateScalar(batch, /*row=*/0)), "X");
    EXPECT_EQ(std::get<std::string>(expr->EvaluateScalar(batch, /*row=*/1)), "bar");
}

TEST_F(FixtureRegexReplace, RespectsInputSelectionVector) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"s", kStr}}),
        MakeColumn<std::string>({"foo", "bar", "foobar", "baz", "afoob"}));
    auto batch = MakeBatchWithSelection(rowGroup, {0, 3});

    auto expr = Rx("foo", "X");
    const auto columnSpan = expr->EvaluateColumn(batch, state_);
    const auto& span = std::get<std::span<const std::string>>(columnSpan);

    ASSERT_EQ(span.size(), 2u);
    EXPECT_EQ(span[0], "X");
    EXPECT_EQ(span[1], "baz");
}

TEST_F(FixtureRegexReplace, EmptyBatchReturnsEmptySpan) {
    auto batch = MakeStringBatch({});
    auto expr = Rx("foo", "X");

    const auto columnSpan = expr->EvaluateColumn(batch, state_);
    const auto& span = std::get<std::span<const std::string>>(columnSpan);

    EXPECT_TRUE(span.empty());
}

TEST_F(FixtureRegexReplace, TrivialAndRegexPathsProduceIdenticalResultsOnLiteralPattern) {
    const std::vector<std::string> input{"foo bar foo baz", "noooo", "fofofofoo"};

    auto trivial = ApplyAll(input, "foo", "X");
    auto regex = ApplyAll(input, R"((foo))", "X");

    EXPECT_EQ(trivial, regex)
        << "L1 fast path must agree with RE2 on identical literal patterns";
}

}  // namespace Columnar::Test
