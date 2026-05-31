#pragma once

#include <exec/aggregate/global/aggregation.h>

#include <exec/aggregate/groupby/aggregation.h>
#include <exec/aggregate/groupby/serializer.h>

#include <exec/aggregate/lib/spec.h>

#include <exec/expression/arithmetic/arithmetic.h>
#include <exec/expression/case_when/case_when.h>
#include <exec/expression/column_ref/column_ref.h>
#include <exec/expression/comparison/comparison.h>
#include <exec/expression/date_trunc/date_trunc.h>
#include <exec/expression/extract/extract.h>
#include <exec/expression/like/like.h>
#include <exec/expression/literal/literal.h>
#include <exec/expression/logical/logical.h>
#include <exec/expression/not/not.h>
#include <exec/expression/regex_replace/regex_replace.h>
#include <exec/expression/string_len/string_len.h>

#include <exec/core/required_columns.h>
#include <exec/filter/filter.h>
#include <exec/interface/expression.h>
#include <exec/sort/top_k.h>
#include <exec/source/table_scan.h>

#include <core/types.h>
#include <util/calendar.h>

#include <memory>

namespace Columnar::Exec {

using GroupByKeys = std::vector<GroupByKey>;
using AggSpecs = std::vector<AggregateSpec>;
using SortKeys = std::vector<SortKey>;

template <typename T, typename... Args>
std::vector<T> MakeVec(Args&&... args) {
    std::vector<T> v;
    v.reserve(sizeof...(args));
    (v.push_back(std::forward<Args>(args)), ...);
    return v;
}

template <typename... Ts>
AggSpecs Aggs(Ts&&... a) {
    return MakeVec<AggregateSpec>(std::forward<Ts>(a)...);
}
template <typename... Ts>
GroupByKeys Keys(Ts&&... a) {
    return MakeVec<GroupByKey>(std::forward<Ts>(a)...);
}
template <typename... Ts>
SortKeys SortBy(Ts&&... a) {
    return MakeVec<SortKey>(std::forward<Ts>(a)...);
}

constexpr auto Bool = Types::LogicalType::BOOL;
constexpr auto I16 = Types::LogicalType::INT16;
constexpr auto I32 = Types::LogicalType::INT32;
constexpr auto I64 = Types::LogicalType::INT64;
constexpr auto Str = Types::LogicalType::STRING;
constexpr auto Ts = Types::LogicalType::TIMESTAMP;
constexpr auto Date = Types::LogicalType::DATE;

// Parse "YYYY-MM-DD" string literal into days since Unix epoch.
// constexpr: evaluated at compile time when called with a string literal.
constexpr int32_t ParseDate(const char* str) {
    const int y = (str[0] - '0') * 1000 + (str[1] - '0') * 100 + (str[2] - '0') * 10 + (str[3] - '0');
    const int m = (str[5] - '0') * 10 + (str[6] - '0');
    const int d = (str[8] - '0') * 10 + (str[9] - '0');
    return Calendar::GregorianToEpochDays(y, m, d);
}

/*
Expression factories
*/

inline auto ColRef(std::string name, Types::LogicalType type) {
    return std::make_unique<ColumnRefExpression>(std::move(name), type);
}

template <typename T>
inline auto Literal(T value, Types::LogicalType type) {
    return std::make_unique<LiteralExpression>(Types::AnyPhysicalType{value}, type);
}

inline auto Cmp(std::unique_ptr<IExpression> l, CompareOp op, std::unique_ptr<IExpression> r) {
    return std::make_unique<ComparisonExpression>(std::move(l), op, std::move(r));
}

template <typename... Ts>
inline auto And(Ts&&... exprs) {
    return std::make_unique<LogicalExpression>(
        LogicalOp::And,
        MakeVec<std::unique_ptr<IExpression>>(std::forward<Ts>(exprs)...));
}

template <typename... Ts>
inline auto Or(Ts&&... exprs) {
    return std::make_unique<LogicalExpression>(
        LogicalOp::Or,
        MakeVec<std::unique_ptr<IExpression>>(std::forward<Ts>(exprs)...));
}

inline auto Not(std::unique_ptr<IExpression> expr) {
    return std::make_unique<NotExpression>(std::move(expr));
}

inline auto Like(std::unique_ptr<IExpression> input, std::string pattern) {
    return std::make_unique<LikeExpression>(std::move(input), pattern);
}

inline auto DateTrunc(std::unique_ptr<IExpression> input, DateTruncUnit unit) {
    return std::make_unique<DateTruncExpression>(std::move(input), unit);
}

inline auto Extract(std::unique_ptr<IExpression> input, ExtractField field) {
    return std::make_unique<ExtractExpression>(std::move(input), field);
}

inline auto Arithm(std::unique_ptr<IExpression> l, ArithmOp op, std::unique_ptr<IExpression> r) {
    return std::make_unique<ArithmeticExpression>(std::move(l), op, std::move(r));
}

inline auto StrLen(std::unique_ptr<IExpression> input) {
    return std::make_unique<StringLenExpression>(std::move(input));
}

inline auto CaseWhen(std::unique_ptr<IExpression> cond, std::unique_ptr<IExpression> then_expr, std::unique_ptr<IExpression> else_expr) {
    return std::make_unique<CaseWhenExpression>(std::move(cond), std::move(then_expr), std::move(else_expr));
}

inline auto RegexReplace(std::unique_ptr<IExpression> input, std::string pattern, std::string replacement) {
    return std::make_unique<RegexReplaceExpression>(std::move(input), std::move(pattern), std::move(replacement));
}

/*
Key/Sort factories
*/
inline GroupByKey Key(std::unique_ptr<IExpression> expr, std::string alias) {
    return {std::move(expr), std::move(alias)};
}

inline SortKey Desc(std::unique_ptr<IExpression> expr) {
    return {std::move(expr), true};
}

inline SortKey Asc(std::unique_ptr<IExpression> expr) {
    return {std::move(expr), false};
}

/*
Operator factories
*/
inline auto Scan(const std::string& path, std::vector<std::string> cols) {
    return std::make_unique<TableScan>(path, RequiredColumns::Only(std::move(cols)));
}

inline auto ScanAll(const std::string& path) {
    return std::make_unique<TableScan>(path, RequiredColumns::All());
}

inline auto Where(std::unique_ptr<IOperator> child, std::unique_ptr<IExpression> pred) {
    return std::make_unique<Filter>(std::move(child), std::move(pred));
}

inline auto GroupBy(std::unique_ptr<IOperator> child, GroupByKeys keys, AggSpecs aggs) {
    return std::make_unique<GroupByAggregation>(std::move(child), std::move(keys), std::move(aggs));
}

inline auto Global(std::unique_ptr<IOperator> child, AggSpecs aggs) {
    return std::make_unique<GlobalAggregation>(std::move(child), std::move(aggs));
}

inline auto OrderLimit(std::unique_ptr<IOperator> child, SortKeys keys, size_t limit, size_t offset = 0) {
    return std::make_unique<TopK>(std::move(child), std::move(keys), limit, offset);
}

}  // namespace Columnar::Exec
