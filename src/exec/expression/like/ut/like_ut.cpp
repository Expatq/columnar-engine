#include <exec/expression/like/like.h>

#include <exec/expression/column_ref/column_ref.h>

#include <exec/core/exec_batch.h>
#include <exec/core/selection_vector.h>
#include <exec/interface/expression.h>

#include <tests/lib/data_builders.h>
#include <tests/lib/exec_batch_builders.h>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Columnar::Test {

namespace {

constexpr auto kI64 = Types::LogicalType::INT64;
constexpr auto kStr = Types::LogicalType::STRING;
constexpr auto kBool = Types::LogicalType::BOOL;

std::unique_ptr<Exec::IExpression> Col(std::string name, Types::LogicalType type) {
    return std::make_unique<Exec::ColumnRefExpression>(std::move(name), type);
}

std::vector<Exec::RowId> AsVec(const Exec::SelectionVector& sel) {
    return {sel.Rows().begin(), sel.Rows().end()};
}

Exec::ExecBatch MakeStringBatch(std::vector<std::string> values) {
    return MakeBatch(MakeRowGroupOf(
        MakeSchema({{"s", kStr}}),
        MakeColumn<std::string>(std::move(values))));
}

std::vector<Exec::RowId> Matches(const Exec::ExecBatch& batch, std::string sqlPattern) {
    Exec::LikeExpression expr(Col("s", kStr), std::move(sqlPattern));
    Exec::SelectionVector sv;
    expr.EvaluateSelection(batch, sv);
    return AsVec(sv);
}

class FixtureLike : public ::testing::Test {
protected:
    Exec::SelectionVector output_;
};

}  // namespace

TEST_F(FixtureLike, KindReportsLike) {
    Exec::LikeExpression expr(Col("s", kStr), "%foo%");

    EXPECT_EQ(expr.Kind(), Exec::ExpressionKind::Like);
}

TEST_F(FixtureLike, ResultTypeIsBool) {
    Exec::LikeExpression expr(Col("s", kStr), "foo");

    EXPECT_EQ(expr.ResultType(), kBool);
}

TEST_F(FixtureLike, RequiredColumnsForwardsFromInput) {
    Exec::LikeExpression expr(Col("url", kStr), "%google%");

    auto cols = expr.RequiredColumns();

    ASSERT_EQ(cols.size(), 1u);
    EXPECT_EQ(cols[0], "url");
}

TEST_F(FixtureLike, ConstructorRejectsNullInput) {
    EXPECT_THROW(Exec::LikeExpression(nullptr, "%a%"), std::invalid_argument);
}

TEST_F(FixtureLike, ConstructorRejectsNonStringInput) {
    EXPECT_THROW(
        Exec::LikeExpression(Col("x", kI64), "%a%"),
        std::invalid_argument);
}

TEST_F(FixtureLike, AlwaysTruePatternMatchesEveryRowIncludingEmpty) {
    auto batch = MakeStringBatch({"", "foo", "bar"});

    EXPECT_EQ(Matches(batch, "%"), (std::vector<Exec::RowId>{0, 1, 2}));
}

TEST_F(FixtureLike, ConsecutivePercentsCollapseToAlwaysTrue) {
    auto batch = MakeStringBatch({"", "foo", "bar"});

    EXPECT_EQ(Matches(batch, "%%%%"), (std::vector<Exec::RowId>{0, 1, 2}));
}

TEST_F(FixtureLike, EmptyPatternMatchesOnlyEmptyString) {
    auto batch = MakeStringBatch({"", "foo", "", "x", "  "});

    EXPECT_EQ(Matches(batch, ""), (std::vector<Exec::RowId>{0, 2}))
        << "SQL: 'foo' LIKE '' is FALSE; only the empty string matches an empty pattern";
}

TEST_F(FixtureLike, ExactPatternRequiresFullEquality) {
    auto batch = MakeStringBatch({"foo", "foobar", "Foo", "", "fooo"});

    EXPECT_EQ(Matches(batch, "foo"), (std::vector<Exec::RowId>{0}));
}

TEST_F(FixtureLike, ExactPatternIsCaseSensitive) {
    auto batch = MakeStringBatch({"Foo", "foo", "FOO"});

    EXPECT_EQ(Matches(batch, "foo"), (std::vector<Exec::RowId>{1}));
}

TEST_F(FixtureLike, PrefixPatternMatchesStringsStartingWithLiteral) {
    auto batch = MakeStringBatch({"foo", "foobar", "barfoo", "fo", "FOO"});

    EXPECT_EQ(Matches(batch, "foo%"), (std::vector<Exec::RowId>{0, 1}));
}

TEST_F(FixtureLike, SuffixPatternMatchesStringsEndingWithLiteral) {
    auto batch = MakeStringBatch({"foo", "barfoo", "foobar", "oo", "FOO"});

    EXPECT_EQ(Matches(batch, "%foo"), (std::vector<Exec::RowId>{0, 1}));
}

TEST_F(FixtureLike, ContainsPatternMatchesAnywhere) {
    auto batch = MakeStringBatch({"foo", "barfoo", "foobar", "barfoobaz", "ba", ""});

    EXPECT_EQ(Matches(batch, "%foo%"), (std::vector<Exec::RowId>{0, 1, 2, 3}));
}

TEST_F(FixtureLike, ContainsPatternTreatsRegexMetacharsAsLiterals) {
    auto batch = MakeStringBatch({"x.y", "xyz", "x*y", "x.y.z", "axby"});

    EXPECT_EQ(Matches(batch, "%.%"), (std::vector<Exec::RowId>{0, 3}))
        << "LIKE '.' must match the literal dot character, not 'any char'";
}

TEST_F(FixtureLike, PrefixSuffixPatternMatchesStartAndEndSimultaneously) {
    auto batch = MakeStringBatch({"foobar", "fooXYZbar", "foo", "bar", "barfoo", ""});

    EXPECT_EQ(Matches(batch, "foo%bar"), (std::vector<Exec::RowId>{0, 1}));
}

TEST_F(FixtureLike, PrefixSuffixRejectsStringTooShortForBothLiterals) {
    auto batch = MakeStringBatch({"abc", "ab", "abxyz", "xyz", "abcxyz"});

    EXPECT_EQ(Matches(batch, "abc%xyz"), (std::vector<Exec::RowId>{4}));
}

TEST_F(FixtureLike, PrefixSuffixHandlesOverlappingAffixes) {
    auto batch = MakeStringBatch({"abab", "ab", "aba", "ababab", "abxab"});

    EXPECT_EQ(Matches(batch, "ab%ab"), (std::vector<Exec::RowId>{0, 3, 4}))
        << "size>=4 and starts/ends with 'ab' suffice; middle may be empty";
}

TEST_F(FixtureLike, MultiContainsAllAnchorsFreeMatchesInOrder) {
    auto batch = MakeStringBatch({
        "axbxc",
        "abc",
        "xaybzc",
        "acb",
        "ab",
    });

    EXPECT_EQ(Matches(batch, "%a%b%c%"), (std::vector<Exec::RowId>{0, 1, 2}));
}

TEST_F(FixtureLike, MultiContainsLeadingAnchorRequiresLiteralPrefix) {
    auto batch = MakeStringBatch({
        "foo_bar_baz",
        "barfoo_baz",
        "foo_baz_bar",
        "foobazbar",
    });

    EXPECT_EQ(Matches(batch, "foo%bar%baz"), (std::vector<Exec::RowId>{0}));
}

TEST_F(FixtureLike, MultiContainsTrailingAnchorRequiresLiteralSuffix) {
    auto batch = MakeStringBatch({
        "x_foo_bar",
        "foo_bar_x",
        "foo_bar",
        "bar_foo",
    });

    EXPECT_EQ(Matches(batch, "%foo%bar"), (std::vector<Exec::RowId>{0, 2}));
}

TEST_F(FixtureLike, MultiContainsRejectsOutOfOrderMatches) {
    auto batch = MakeStringBatch({"abc", "cba", "axbxc", "cxbxa"});

    EXPECT_EQ(Matches(batch, "%a%b%c%"), (std::vector<Exec::RowId>{0, 2}))
        << "segments must appear left-to-right in the haystack";
}

TEST_F(FixtureLike, MultiContainsOverlappingNeedlesAdvanceWithoutDoubleCounting) {
    auto batch = MakeStringBatch({"abc", "abcabc", "abc_abc", "ab_abc"});

    EXPECT_EQ(Matches(batch, "%abc%abc%"), (std::vector<Exec::RowId>{1, 2}))
        << "second 'abc' must start after the first match, not overlap it";
}

TEST_F(FixtureLike, UnderscoreMatchesExactlyOneCharacter) {
    auto batch = MakeStringBatch({"a", "ab", "abc", "", "x"});

    EXPECT_EQ(Matches(batch, "_"), (std::vector<Exec::RowId>{0, 4}));
}

TEST_F(FixtureLike, UnderscoreWithLiteralAfterIt) {
    auto batch = MakeStringBatch({"ba", "ca", "a", "xa", "bba"});

    EXPECT_EQ(Matches(batch, "_a"), (std::vector<Exec::RowId>{0, 1, 3}));
}

TEST_F(FixtureLike, UnderscoreInMiddleRequiresExactlyOneCharacterBetweenLiterals) {
    auto batch = MakeStringBatch({"ac", "abc", "axc", "abbc", "a_c"});

    EXPECT_EQ(Matches(batch, "a_c"), (std::vector<Exec::RowId>{1, 2, 4}));
}

TEST_F(FixtureLike, MixedUnderscoreAndPercentMatchAnyNonEmptyString) {
    auto batch = MakeStringBatch({"", "a", "abc", "longer string here"});

    EXPECT_EQ(Matches(batch, "%_%"), (std::vector<Exec::RowId>{1, 2, 3}));
}

TEST_F(FixtureLike, NfaVectorPathHandlesPatternsBeyondBitsetThreshold) {
    std::string longPattern(80, 'a');
    longPattern.back() = '_';
    std::string longMatching(79, 'a');
    longMatching.push_back('x');

    auto batch = MakeStringBatch({
        longMatching,
        std::string(80, 'a'),
        std::string(79, 'a'),
        std::string(81, 'a'),
    });

    EXPECT_EQ(Matches(batch, longPattern), (std::vector<Exec::RowId>{0, 1}))
        << "pattern length 80 must dispatch to the std::vector NFA path and still be correct";
}

TEST_F(FixtureLike, ContainsAgainstEmptyStringFindsNothing) {
    auto batch = MakeStringBatch({"", ""});

    EXPECT_TRUE(Matches(batch, "%foo%").empty());
}

TEST_F(FixtureLike, PrefixAgainstShorterStringDoesNotMatch) {
    auto batch = MakeStringBatch({"fo", "f", ""});

    EXPECT_TRUE(Matches(batch, "foo%").empty());
}

TEST_F(FixtureLike, EmptyBatchProducesEmptyOutput) {
    auto batch = MakeStringBatch({});

    EXPECT_TRUE(Matches(batch, "%foo%").empty());
}

TEST_F(FixtureLike, EvaluateSelectionClearsOutputBeforePopulating) {
    auto batch = MakeStringBatch({"foo", "bar", "foobar"});
    output_.Push(99);
    output_.Push(100);

    Exec::LikeExpression expr(Col("s", kStr), "%foo%");
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{0, 2}));
}

TEST_F(FixtureLike, RespectsInputSelectionVector) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"s", kStr}}),
        MakeColumn<std::string>({"foo", "bar", "foobar", "baz", "afoob"}));
    auto batch = MakeBatchWithSelection(rowGroup, {1, 3});

    Exec::LikeExpression expr(Col("s", kStr), "%foo%");
    expr.EvaluateSelection(batch, output_);

    EXPECT_TRUE(output_.Empty())
        << "rows 0,2,4 match the pattern but are outside input selection";
}

TEST_F(FixtureLike, PreservesInputSelectionOrderForMatchingRows) {
    auto rowGroup = MakeRowGroupOf(
        MakeSchema({{"s", kStr}}),
        MakeColumn<std::string>({"a", "foo", "b", "foobar", "c", "barfoo"}));
    auto batch = MakeBatchWithSelection(rowGroup, {5, 1, 3});

    Exec::LikeExpression expr(Col("s", kStr), "%foo%");
    expr.EvaluateSelection(batch, output_);

    EXPECT_EQ(AsVec(output_), (std::vector<Exec::RowId>{5, 1, 3}));
}

TEST_F(FixtureLike, ClickBenchStyleGoogleContainsMatchesUrls) {
    auto batch = MakeStringBatch({
        "http://www.google.com/search?q=clickbench",
        "https://yandex.ru/",
        "http://google.com",
        "https://www.bing.com/search?q=google",
        "ftp://files/google_doc.pdf",
    });

    EXPECT_EQ(Matches(batch, "%google%"),
              (std::vector<Exec::RowId>{0, 2, 3, 4}));
}

TEST_F(FixtureLike, ClickBenchStyleDottedDomainPatternIsLiteral) {
    auto batch = MakeStringBatch({
        "http://www.google.com/",
        "http://google.com/",
        "http://foogoogleX/",
        "http://docs.google.com/sheets",
    });

    EXPECT_EQ(Matches(batch, "%.google.%"),
              (std::vector<Exec::RowId>{0, 3}))
        << "dots are literal: 'foogoogleX' does not match even though it contains 'google'";
}

}  // namespace Columnar::Test
