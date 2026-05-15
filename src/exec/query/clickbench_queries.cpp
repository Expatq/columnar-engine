#include "clickbench_queries.h"
#include "query_helpers.h"

#include <exec/source/metadata_scan.h>

#include <core/column.h>
#include <core/row_group.h>
#include <core/schema.h>
#include <core/types.h>

namespace Columnar::Exec {

namespace {

using QueryBuilder = std::unique_ptr<Columnar::Exec::IOperator> (*)(const std::string&);

constexpr QueryBuilder kClickBenchBuilders[] = {
    &Columnar::Exec::BuildQ0,
    &Columnar::Exec::BuildQ1,
    &Columnar::Exec::BuildQ2,
    &Columnar::Exec::BuildQ3,
    &Columnar::Exec::BuildQ4,
    &Columnar::Exec::BuildQ5,
    &Columnar::Exec::BuildQ6,
    &Columnar::Exec::BuildQ7,
    &Columnar::Exec::BuildQ8,
    &Columnar::Exec::BuildQ9,
    &Columnar::Exec::BuildQ10,
    &Columnar::Exec::BuildQ11,
    &Columnar::Exec::BuildQ12,
    &Columnar::Exec::BuildQ13,
    &Columnar::Exec::BuildQ14,
    &Columnar::Exec::BuildQ15,
    &Columnar::Exec::BuildQ16,
    &Columnar::Exec::BuildQ17,
    &Columnar::Exec::BuildQ18,
    &Columnar::Exec::BuildQ19,
    &Columnar::Exec::BuildQ20,
    &Columnar::Exec::BuildQ21,
    &Columnar::Exec::BuildQ22,
    &Columnar::Exec::BuildQ23,
    &Columnar::Exec::BuildQ24,
    &Columnar::Exec::BuildQ25,
    &Columnar::Exec::BuildQ26,
    &Columnar::Exec::BuildQ27,
    &Columnar::Exec::BuildQ28,
    &Columnar::Exec::BuildQ29,
    &Columnar::Exec::BuildQ30,
    &Columnar::Exec::BuildQ31,
    &Columnar::Exec::BuildQ32,
    &Columnar::Exec::BuildQ33,
    &Columnar::Exec::BuildQ34,
    &Columnar::Exec::BuildQ35,
    &Columnar::Exec::BuildQ36,
    &Columnar::Exec::BuildQ37,
    &Columnar::Exec::BuildQ38,
    &Columnar::Exec::BuildQ39,
    &Columnar::Exec::BuildQ40,
    &Columnar::Exec::BuildQ41,
    &Columnar::Exec::BuildQ42,
};

}  // namespace

// Q0: SELECT COUNT(*) FROM hits
std::unique_ptr<IOperator> BuildQ0(const std::string& path) {
    return std::make_unique<MetadataScan>(
        path,
        std::vector<MetadataColumn>{{MetadataField::TotalRowCount, "count", I64}});
}

// Q1: SELECT COUNT(*) FROM hits WHERE AdvEngineID <> 0
std::unique_ptr<IOperator> BuildQ1(const std::string& path) {
    return Global(
        Where(Scan(path, {"AdvEngineID"}),
              Cmp(ColRef("AdvEngineID", I16), CompareOp::NotEq, Literal(int16_t{0}, I16))),
        Aggs(CountStar("count")));
}

// Q2: SELECT SUM(AdvEngineID), COUNT(*), AVG(ResolutionWidth) FROM hits
std::unique_ptr<IOperator> BuildQ2(const std::string& path) {
    return Global(Scan(path, {"AdvEngineID", "ResolutionWidth"}), Aggs(
                                                                      Sum(ColRef("AdvEngineID", I16), "sum_adv_engine_id", I64),
                                                                      CountStar("count"),
                                                                      Avg(ColRef("ResolutionWidth", I16), "avg_resolution_width", I64)));
}

// Q3: SELECT AVG(UserID) FROM hits
std::unique_ptr<IOperator> BuildQ3(const std::string& path) {
    return Global(Scan(path, {"UserID"}),
                  Aggs(Avg(ColRef("UserID", I64), "avg_user_id", I64)));
}

// Q4: SELECT COUNT(DISTINCT UserID) FROM hits
std::unique_ptr<IOperator> BuildQ4(const std::string& path) {
    return Global(Scan(path, {"UserID"}),
                  Aggs(CountDistinct(ColRef("UserID", I64), "count_distinct_user_id")));
}

// Q5: SELECT COUNT(DISTINCT SearchPhrase) FROM hits
std::unique_ptr<IOperator> BuildQ5(const std::string& path) {
    return Global(Scan(path, {"SearchPhrase"}),
                  Aggs(CountDistinct(ColRef("SearchPhrase", Str), "count_distinct_phrase")));
}

// Q6: SELECT MIN(EventDate), MAX(EventDate) FROM hits
std::unique_ptr<IOperator> BuildQ6(const std::string& path) {
    return Global(Scan(path, {"EventDate"}), Aggs(
                                                 Min(ColRef("EventDate", Date), "min_event_date"),
                                                 Max(ColRef("EventDate", Date), "max_event_date")));
}

// Q7: SELECT AdvEngineID, COUNT(*) FROM hits WHERE AdvEngineID <> 0
//     GROUP BY AdvEngineID ORDER BY COUNT(*) DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ7(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"AdvEngineID"}),
                  Cmp(ColRef("AdvEngineID", I16), CompareOp::NotEq, Literal(int16_t{0}, I16))),
            Keys(Key(ColRef("AdvEngineID", I16), "AdvEngineID")),
            Aggs(CountStar("count"))),
        SortBy(Desc(ColRef("count", I64))), 10);
}

// Q8: SELECT RegionID, COUNT(DISTINCT UserID) AS u FROM hits
//     GROUP BY RegionID ORDER BY u DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ8(const std::string& path) {
    return OrderLimit(
        GroupBy(Scan(path, {"RegionID", "UserID"}),
                Keys(Key(ColRef("RegionID", I32), "RegionID")),
                Aggs(CountDistinct(ColRef("UserID", I64), "u"))),
        SortBy(Desc(ColRef("u", I64))), 10);
}

// Q9: SELECT RegionID, SUM(AdvEngineID), COUNT(*) AS c, AVG(ResolutionWidth), COUNT(DISTINCT UserID)
//      FROM hits GROUP BY RegionID ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ9(const std::string& path) {
    return OrderLimit(
        GroupBy(Scan(path, {"RegionID", "AdvEngineID", "ResolutionWidth", "UserID"}),
                Keys(Key(ColRef("RegionID", I32), "RegionID")),
                Aggs(
                    Sum(ColRef("AdvEngineID", I16), "sum_adv", I64),
                    CountStar("c"),
                    Avg(ColRef("ResolutionWidth", I16), "avg_width", I64),
                    CountDistinct(ColRef("UserID", I64), "u"))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q10: SELECT MobilePhoneModel, COUNT(DISTINCT UserID) AS u FROM hits
//      WHERE MobilePhoneModel != '' GROUP BY MobilePhoneModel ORDER BY u DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ10(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"MobilePhoneModel", "UserID"}),
                  Cmp(ColRef("MobilePhoneModel", Str), CompareOp::NotEq, Literal(std::string{""}, Str))),
            Keys(Key(ColRef("MobilePhoneModel", Str), "MobilePhoneModel")),
            Aggs(CountDistinct(ColRef("UserID", I64), "u"))),
        SortBy(Desc(ColRef("u", I64))), 10);
}

// Q11: SELECT MobilePhone, MobilePhoneModel, COUNT(DISTINCT UserID) AS u FROM hits
//      WHERE MobilePhoneModel != '' GROUP BY MobilePhone, MobilePhoneModel ORDER BY u DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ11(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"MobilePhone", "MobilePhoneModel", "UserID"}),
                  Cmp(ColRef("MobilePhoneModel", Str), CompareOp::NotEq, Literal(std::string{""}, Str))),
            Keys(
                Key(ColRef("MobilePhone", I16), "MobilePhone"),
                Key(ColRef("MobilePhoneModel", Str), "MobilePhoneModel")),
            Aggs(CountDistinct(ColRef("UserID", I64), "u"))),
        SortBy(Desc(ColRef("u", I64))), 10);
}

// Q12: SELECT SearchPhrase, COUNT(*) AS c FROM hits
//      WHERE SearchPhrase != '' GROUP BY SearchPhrase ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ12(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"SearchPhrase"}),
                  Cmp(ColRef("SearchPhrase", Str), CompareOp::NotEq, Literal(std::string{""}, Str))),
            Keys(Key(ColRef("SearchPhrase", Str), "SearchPhrase")),
            Aggs(CountStar("c"))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q13: SELECT SearchPhrase, COUNT(DISTINCT UserID) AS u FROM hits
//      WHERE SearchPhrase != '' GROUP BY SearchPhrase ORDER BY u DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ13(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"SearchPhrase", "UserID"}),
                  Cmp(ColRef("SearchPhrase", Str), CompareOp::NotEq, Literal(std::string{""}, Str))),
            Keys(Key(ColRef("SearchPhrase", Str), "SearchPhrase")),
            Aggs(CountDistinct(ColRef("UserID", I64), "u"))),
        SortBy(Desc(ColRef("u", I64))), 10);
}

// Q14: SELECT SearchEngineID, SearchPhrase, COUNT(*) AS c FROM hits
//      WHERE SearchPhrase != '' GROUP BY SearchEngineID, SearchPhrase ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ14(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"SearchEngineID", "SearchPhrase"}),
                  Cmp(ColRef("SearchPhrase", Str), CompareOp::NotEq, Literal(std::string{""}, Str))),
            Keys(
                Key(ColRef("SearchEngineID", I16), "SearchEngineID"),
                Key(ColRef("SearchPhrase", Str), "SearchPhrase")),
            Aggs(CountStar("c"))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q15: SELECT UserID, COUNT(*) AS c FROM hits GROUP BY UserID ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ15(const std::string& path) {
    return OrderLimit(
        GroupBy(Scan(path, {"UserID"}),
                Keys(Key(ColRef("UserID", I64), "UserID")),
                Aggs(CountStar("c"))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q16: SELECT UserID, SearchPhrase, COUNT(*) AS c FROM hits
//      GROUP BY UserID, SearchPhrase ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ16(const std::string& path) {
    return OrderLimit(
        GroupBy(Scan(path, {"UserID", "SearchPhrase"}),
                Keys(
                    Key(ColRef("UserID", I64), "UserID"),
                    Key(ColRef("SearchPhrase", Str), "SearchPhrase")),
                Aggs(CountStar("c"))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q17: SELECT UserID, SearchPhrase, COUNT(*) AS c FROM hits
//      GROUP BY UserID, SearchPhrase ORDER BY c DESC LIMIT 10 OFFSET 1000
std::unique_ptr<IOperator> BuildQ17(const std::string& path) {
    return OrderLimit(
        GroupBy(Scan(path, {"UserID", "SearchPhrase"}),
                Keys(
                    Key(ColRef("UserID", I64), "UserID"),
                    Key(ColRef("SearchPhrase", Str), "SearchPhrase")),
                Aggs(CountStar("c"))),
        SortBy(Desc(ColRef("c", I64))), 10, 1000);
}

// Q18: SELECT UserID, toStartOfMinute(EventTime) AS m, SearchPhrase, COUNT(*) AS c, COUNT(DISTINCT UserID)
//      FROM hits GROUP BY UserID, m, SearchPhrase ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ18(const std::string& path) {
    return OrderLimit(
        GroupBy(Scan(path, {"UserID", "EventTime", "SearchPhrase"}),
                Keys(
                    Key(ColRef("UserID", I64), "UserID"),
                    Key(DateTrunc(ColRef("EventTime", Ts), DateTruncUnit::Minute), "m"),
                    Key(ColRef("SearchPhrase", Str), "SearchPhrase")),
                Aggs(
                    CountStar("c"),
                    CountDistinct(ColRef("UserID", I64), "u"))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q19: SELECT UserID, ClientIP, COUNT(*) AS c, SUM(IsRefresh), AVG(ResolutionWidth)
//      FROM hits GROUP BY UserID, ClientIP ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ19(const std::string& path) {
    return OrderLimit(
        GroupBy(Scan(path, {"UserID", "ClientIP", "IsRefresh", "ResolutionWidth"}),
                Keys(
                    Key(ColRef("UserID", I64), "UserID"),
                    Key(ColRef("ClientIP", I32), "ClientIP")),
                Aggs(
                    CountStar("c"),
                    Sum(ColRef("IsRefresh", I16), "sum_refresh", I64),
                    Avg(ColRef("ResolutionWidth", I16), "avg_width", I64))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q20: SELECT count(*) FROM hits WHERE URL LIKE '%google%'
std::unique_ptr<IOperator> BuildQ20(const std::string& path) {
    return Global(
        Where(Scan(path, {"URL"}), Like(ColRef("URL", Str), "%google%")),
        Aggs(CountStar("count")));
}

// Q21: SELECT SearchPhrase, min(URL), count(*) AS c
//      FROM hits WHERE URL LIKE '%google%' AND SearchPhrase != ''
//      GROUP BY SearchPhrase ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ21(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"SearchPhrase", "URL"}),
                  And(Like(ColRef("URL", Str), "%google%"),
                      Cmp(ColRef("SearchPhrase", Str), CompareOp::NotEq,
                          Literal(std::string{""}, Str)))),
            Keys(Key(ColRef("SearchPhrase", Str), "SearchPhrase")),
            Aggs(Min(ColRef("URL", Str), "min_url"), CountStar("c"))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q22: SELECT SearchPhrase, min(URL), min(Title), count(*) AS c
//      FROM hits WHERE Title LIKE '%Google%' AND URL NOT LIKE '%.google.%'
//              AND SearchPhrase != ''
//      GROUP BY SearchPhrase ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ22(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"SearchPhrase", "URL", "Title"}),
                  And(Like(ColRef("Title", Str), "%Google%"),
                      Cmp(ColRef("SearchPhrase", Str), CompareOp::NotEq,
                          Literal(std::string{""}, Str)),
                      Not(Like(ColRef("URL", Str), "%.google.%")))),
            Keys(Key(ColRef("SearchPhrase", Str), "SearchPhrase")),
            Aggs(Min(ColRef("URL", Str), "min_url"),
                 Min(ColRef("Title", Str), "min_title"),
                 CountStar("c"))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q23: SELECT * FROM hits WHERE URL LIKE '%google%' ORDER BY EventTime LIMIT 10
std::unique_ptr<IOperator> BuildQ23(const std::string& path) {
    return OrderLimit(
        Where(ScanAll(path), Like(ColRef("URL", Str), "%google%")),
        SortBy(Asc(ColRef("EventTime", Ts))), 10);
}

// Q24: SELECT SearchPhrase, EventTime FROM hits
//      WHERE SearchPhrase != '' ORDER BY EventTime LIMIT 10
std::unique_ptr<IOperator> BuildQ24(const std::string& path) {
    return OrderLimit(
        Where(Scan(path, {"SearchPhrase", "EventTime"}),
              Cmp(ColRef("SearchPhrase", Str), CompareOp::NotEq,
                  Literal(std::string{""}, Str))),
        SortBy(Asc(ColRef("EventTime", Ts))), 10);
}

// Q25: SELECT SearchPhrase FROM hits
//      WHERE SearchPhrase != '' ORDER BY SearchPhrase LIMIT 10
std::unique_ptr<IOperator> BuildQ25(const std::string& path) {
    return OrderLimit(
        Where(Scan(path, {"SearchPhrase"}),
              Cmp(ColRef("SearchPhrase", Str), CompareOp::NotEq,
                  Literal(std::string{""}, Str))),
        SortBy(Asc(ColRef("SearchPhrase", Str))), 10);
}

// Q26: SELECT SearchPhrase FROM hits
//      WHERE SearchPhrase != '' ORDER BY EventTime, SearchPhrase LIMIT 10
std::unique_ptr<IOperator> BuildQ26(const std::string& path) {
    return OrderLimit(
        Where(Scan(path, {"SearchPhrase", "EventTime"}),
              Cmp(ColRef("SearchPhrase", Str), CompareOp::NotEq,
                  Literal(std::string{""}, Str))),
        SortBy(Asc(ColRef("EventTime", Ts)), Asc(ColRef("SearchPhrase", Str))), 10);
}

// Q27: SELECT avg(length(URL)) AS l, CounterID
//      FROM hits GROUP BY CounterID HAVING count(*) > 100000
//      ORDER BY l DESC LIMIT 25
std::unique_ptr<IOperator> BuildQ27(const std::string& path) {
    return OrderLimit(
        Where(
            GroupBy(
                Scan(path, {"CounterID", "URL"}),
                Keys(Key(ColRef("CounterID", I32), "CounterID")),
                Aggs(Avg(StrLen(ColRef("URL", Str)), "l", I64), CountStar("cnt"))),
            Cmp(ColRef("cnt", I64), CompareOp::Gt, Literal(int64_t{100000}, I64))),
        SortBy(Desc(ColRef("l", I64))), 25);
}

// Q28: SELECT REGEXP_REPLACE(Referer, '^https?://(?:www\.)?([^/]+)/.*$', '\1') AS k,
//             avg(length(Referer)) AS l, count(*) AS c, min(Referer)
//      FROM hits WHERE Referer != ''
//      GROUP BY k ORDER BY l DESC LIMIT 25
std::unique_ptr<IOperator> BuildQ28(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"Referer"}),
                  Cmp(ColRef("Referer", Str), CompareOp::NotEq,
                      Literal(std::string{""}, Str))),
            Keys(Key(RegexReplace(ColRef("Referer", Str),
                                  "^https?://(?:www\\.)?([^/]+)/.*$", "$1"),
                     "k")),
            Aggs(Avg(StrLen(ColRef("Referer", Str)), "l", I64),
                 CountStar("c"),
                 Min(ColRef("Referer", Str), "min_referer"))),
        SortBy(Desc(ColRef("l", I64))), 25);
}

// Q29: SELECT SUM(ResolutionWidth), SUM(ResolutionWidth+1), ..., SUM(ResolutionWidth+89)
//      FROM hits
//
// SUM(w + n) = SUM(w) + n * COUNT(*) — один проход, два агрегата,
// остальные 88 значений выводятся скалярным сложением на единственной строке результата.
std::unique_ptr<IOperator> BuildQ29(const std::string& path) {
    class ExpandSumsOperator : public IOperator {
    public:
        explicit ExpandSumsOperator(std::unique_ptr<IOperator> child)
            : child_(std::move(child)) {
        }

        void Open() override {
            child_->Open();
            produced_ = false;
        }
        void Close() noexcept override {
            child_->Close();
        }

        bool Next(ExecBatch& out) override {
            if (produced_)
                return false;
            ExecBatch aggResult;
            if (!child_->Next(aggResult))
                return false;

            const int64_t baseSum = aggResult.rowGroup->FindColumn("base_sum")->GetTypedData<int64_t>()[0];
            const int64_t rowCount = aggResult.rowGroup->FindColumn("row_count")->GetTypedData<int64_t>()[0];

            Schema schema;
            std::vector<Column> columns;
            columns.reserve(90);
            for (int n = 0; n < 90; ++n) {
                schema.AddColumn("w" + std::to_string(n), Types::LogicalType::INT64);
                columns.emplace_back(std::vector<int64_t>{baseSum + int64_t(n) * rowCount},
                                     Types::PhysicalType::INT64);
            }
            out.rowGroup = std::make_shared<RowGroup>(std::move(schema), std::move(columns));
            out.rowCount = 1;
            produced_ = true;
            return true;
        }

    private:
        std::unique_ptr<IOperator> child_;
        bool produced_ = false;
    };

    return std::make_unique<ExpandSumsOperator>(
        Global(Scan(path, {"ResolutionWidth"}),
               Aggs(Sum(ColRef("ResolutionWidth", I16), "base_sum", I64),
                    CountStar("row_count"))));
}

// Q30: SELECT SearchEngineID, ClientIP, count(*) AS c, sum(IsRefresh), avg(ResolutionWidth)
//      FROM hits WHERE SearchPhrase != ''
//      GROUP BY SearchEngineID, ClientIP ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ30(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"SearchEngineID", "ClientIP", "IsRefresh",
                              "ResolutionWidth", "SearchPhrase"}),
                  Cmp(ColRef("SearchPhrase", Str), CompareOp::NotEq,
                      Literal(std::string{""}, Str))),
            Keys(Key(ColRef("SearchEngineID", I16), "SearchEngineID"),
                 Key(ColRef("ClientIP", I32), "ClientIP")),
            Aggs(CountStar("c"),
                 Sum(ColRef("IsRefresh", I16), "sum_refresh", I64),
                 Avg(ColRef("ResolutionWidth", I16), "avg_width", I64))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q31: SELECT WatchID, ClientIP, count(*) AS c, sum(IsRefresh), avg(ResolutionWidth)
//      FROM hits WHERE SearchPhrase != ''
//      GROUP BY WatchID, ClientIP ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ31(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"WatchID", "ClientIP", "IsRefresh",
                              "ResolutionWidth", "SearchPhrase"}),
                  Cmp(ColRef("SearchPhrase", Str), CompareOp::NotEq,
                      Literal(std::string{""}, Str))),
            Keys(Key(ColRef("WatchID", I64), "WatchID"),
                 Key(ColRef("ClientIP", I32), "ClientIP")),
            Aggs(CountStar("c"),
                 Sum(ColRef("IsRefresh", I16), "sum_refresh", I64),
                 Avg(ColRef("ResolutionWidth", I16), "avg_width", I64))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q32: SELECT WatchID, ClientIP, count(*) AS c, sum(IsRefresh), avg(ResolutionWidth)
//      FROM hits GROUP BY WatchID, ClientIP ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ32(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Scan(path, {"WatchID", "ClientIP", "IsRefresh", "ResolutionWidth"}),
            Keys(Key(ColRef("WatchID", I64), "WatchID"),
                 Key(ColRef("ClientIP", I32), "ClientIP")),
            Aggs(CountStar("c"),
                 Sum(ColRef("IsRefresh", I16), "sum_refresh", I64),
                 Avg(ColRef("ResolutionWidth", I16), "avg_width", I64))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q33: SELECT URL, count(*) AS c FROM hits GROUP BY URL ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ33(const std::string& path) {
    return OrderLimit(
        GroupBy(Scan(path, {"URL"}),
                Keys(Key(ColRef("URL", Str), "URL")),
                Aggs(CountStar("c"))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q34: SELECT 1, URL, count(*) AS c FROM hits GROUP BY 1, URL ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ34(const std::string& path) {
    return OrderLimit(
        GroupBy(Scan(path, {"URL"}),
                Keys(Key(Literal(int32_t{1}, I32), "one"),
                     Key(ColRef("URL", Str), "URL")),
                Aggs(CountStar("c"))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q35: SELECT ClientIP, ClientIP-1 AS k1, ClientIP-2 AS k2, ClientIP-3 AS k3, count(*) AS c
//      FROM hits GROUP BY ClientIP, k1, k2, k3 ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ35(const std::string& path) {
    return OrderLimit(
        GroupBy(Scan(path, {"ClientIP"}),
                Keys(Key(ColRef("ClientIP", I32), "ClientIP"),
                     Key(Arithm(ColRef("ClientIP", I32), ArithmOp::Sub,
                                Literal(int32_t{1}, I32)),
                         "k1"),
                     Key(Arithm(ColRef("ClientIP", I32), ArithmOp::Sub,
                                Literal(int32_t{2}, I32)),
                         "k2"),
                     Key(Arithm(ColRef("ClientIP", I32), ArithmOp::Sub,
                                Literal(int32_t{3}, I32)),
                         "k3")),
                Aggs(CountStar("c"))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q36: SELECT URL, count(*) AS PageViews FROM hits
//      WHERE CounterID = 62 AND EventDate >= '2013-07-01' AND EventDate <= '2013-07-31'
//        AND URL != ''
//      GROUP BY URL ORDER BY PageViews DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ36(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"CounterID", "EventDate", "URL"}),
                  And(Cmp(ColRef("CounterID", I32), CompareOp::Eq,
                          Literal(int32_t{62}, I32)),
                      Cmp(ColRef("EventDate", Date), CompareOp::Gte,
                          Literal(ParseDate("2013-07-01"), Date)),
                      Cmp(ColRef("EventDate", Date), CompareOp::Lte,
                          Literal(ParseDate("2013-07-31"), Date)),
                      Cmp(ColRef("URL", Str), CompareOp::NotEq,
                          Literal(std::string{""}, Str)))),
            Keys(Key(ColRef("URL", Str), "URL")),
            Aggs(CountStar("PageViews"))),
        SortBy(Desc(ColRef("PageViews", I64))), 10);
}

// Q37: SELECT Title, count(*) AS PageViews FROM hits
//      WHERE CounterID = 62 AND EventDate >= '2013-07-01' AND EventDate <= '2013-07-31'
//        AND Title != ''
//      GROUP BY Title ORDER BY PageViews DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ37(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"CounterID", "EventDate", "Title"}),
                  And(Cmp(ColRef("CounterID", I32), CompareOp::Eq,
                          Literal(int32_t{62}, I32)),
                      Cmp(ColRef("EventDate", Date), CompareOp::Gte,
                          Literal(ParseDate("2013-07-01"), Date)),
                      Cmp(ColRef("EventDate", Date), CompareOp::Lte,
                          Literal(ParseDate("2013-07-31"), Date)),
                      Cmp(ColRef("Title", Str), CompareOp::NotEq,
                          Literal(std::string{""}, Str)))),
            Keys(Key(ColRef("Title", Str), "Title")),
            Aggs(CountStar("PageViews"))),
        SortBy(Desc(ColRef("PageViews", I64))), 10);
}

// Q38: SELECT URL, count(*) AS PageViews FROM hits
//      WHERE CounterID = 62 AND EventDate >= '2013-07-01' AND EventDate <= '2013-07-31'
//        AND IsLink != 0 AND IsDownload = 0 AND URL != ''
//      GROUP BY URL ORDER BY PageViews DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ38(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"CounterID", "EventDate", "IsLink", "IsDownload", "URL"}),
                  And(Cmp(ColRef("CounterID", I32), CompareOp::Eq,
                          Literal(int32_t{62}, I32)),
                      Cmp(ColRef("EventDate", Date), CompareOp::Gte,
                          Literal(ParseDate("2013-07-01"), Date)),
                      Cmp(ColRef("EventDate", Date), CompareOp::Lte,
                          Literal(ParseDate("2013-07-31"), Date)),
                      Cmp(ColRef("IsLink", Bool), CompareOp::NotEq,
                          Literal(uint8_t{0}, Bool)),
                      Cmp(ColRef("IsDownload", Bool), CompareOp::Eq,
                          Literal(uint8_t{0}, Bool)),
                      Cmp(ColRef("URL", Str), CompareOp::NotEq,
                          Literal(std::string{""}, Str)))),
            Keys(Key(ColRef("URL", Str), "URL")),
            Aggs(CountStar("PageViews"))),
        SortBy(Desc(ColRef("PageViews", I64))), 10);
}

// Q39: SELECT TraficSourceID, SearchEngineID, AdvEngineID,
//             CASE WHEN (SearchEngineID = 0 AND AdvEngineID = 0) THEN Referer ELSE '·' END AS Src,
//             URL AS Dst, count(*) AS PageViews
//      FROM hits GROUP BY TraficSourceID, SearchEngineID, AdvEngineID, Src, URL
//      ORDER BY PageViews DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ39(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Scan(path, {"TraficSourceID", "SearchEngineID", "AdvEngineID", "Referer", "URL"}),
            Keys(Key(ColRef("TraficSourceID", I16), "TraficSourceID"),
                 Key(ColRef("SearchEngineID", I16), "SearchEngineID"),
                 Key(ColRef("AdvEngineID", I16), "AdvEngineID"),
                 Key(CaseWhen(
                         And(Cmp(ColRef("SearchEngineID", I16), CompareOp::Eq,
                                 Literal(int16_t{0}, I16)),
                             Cmp(ColRef("AdvEngineID", I16), CompareOp::Eq,
                                 Literal(int16_t{0}, I16))),
                         ColRef("Referer", Str),
                         Literal(std::string{"\xC2\xB7"}, Str)),
                     "Src"),
                 Key(ColRef("URL", Str), "Dst")),
            Aggs(CountStar("PageViews"))),
        SortBy(Desc(ColRef("PageViews", I64))), 10);
}

// Q40: SELECT TraficSourceID, count(*) AS c FROM hits
//      WHERE TraficSourceID IN (-1, 6)
//      GROUP BY TraficSourceID ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ40(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"TraficSourceID"}),
                  Or(Cmp(ColRef("TraficSourceID", I16), CompareOp::Eq,
                         Literal(int16_t{-1}, I16)),
                     Cmp(ColRef("TraficSourceID", I16), CompareOp::Eq,
                         Literal(int16_t{6}, I16)))),
            Keys(Key(ColRef("TraficSourceID", I16), "TraficSourceID")),
            Aggs(CountStar("c"))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q41: SELECT WindowClientWidth, URLHash, count(*) AS c FROM hits
//      WHERE CounterID = 62
//      GROUP BY WindowClientWidth, URLHash ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ41(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"CounterID", "WindowClientWidth", "URLHash"}),
                  Cmp(ColRef("CounterID", I32), CompareOp::Eq,
                      Literal(int32_t{62}, I32))),
            Keys(Key(ColRef("WindowClientWidth", I16), "WindowClientWidth"),
                 Key(ColRef("URLHash", I64), "URLHash")),
            Aggs(CountStar("c"))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q42: SELECT toStartOfMinute(EventTime) AS M, count(*) AS c FROM hits
//      WHERE EventDate >= '2014-01-01' AND EventDate <= '2014-01-31'
//      GROUP BY M ORDER BY M LIMIT 10
std::unique_ptr<IOperator> BuildQ42(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"EventTime", "EventDate"}),
                  And(Cmp(ColRef("EventDate", Date), CompareOp::Gte,
                          Literal(ParseDate("2014-01-01"), Date)),
                      Cmp(ColRef("EventDate", Date), CompareOp::Lte,
                          Literal(ParseDate("2014-01-31"), Date)))),
            Keys(Key(DateTrunc(ColRef("EventTime", Ts), DateTruncUnit::Minute), "M")),
            Aggs(CountStar("c"))),
        SortBy(Asc(ColRef("M", I64))), 10);
}

std::unique_ptr<IOperator> BuildClickBenchQuery(const std::string& iyxPath,
                                                size_t queryId) {
    if (queryId >= kClickBenchQueryCount || queryId < 0) {
        throw std::out_of_range("ClickBench query id is out of range: " +
                                std::to_string(queryId));
    }
    return kClickBenchBuilders[queryId](iyxPath);
}

}  // namespace Columnar::Exec
