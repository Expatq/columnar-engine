#include "clickbench_queries.h"
#include "query_helpers.h"

#include <exec/source/metadata_scan.h>

namespace Columnar::Exec {

// Q1: SELECT COUNT(*) FROM hits
std::unique_ptr<IOperator> BuildQ1(const std::string& path) {
    return std::make_unique<MetadataScan>(
        path,
        std::vector<MetadataColumn>{{MetadataField::TotalRowCount, "count", I64}});
}

// Q2: SELECT COUNT(*) FROM hits WHERE AdvEngineID <> 0
std::unique_ptr<IOperator> BuildQ2(const std::string& path) {
    return Global(
        Where(Scan(path, {"AdvEngineID"}),
              Cmp(ColRef("AdvEngineID", I16), CompareOp::NotEq, Literal(int16_t{0}, I16))),
        Aggs(CountStar("count")));
}

// Q3: SELECT SUM(AdvEngineID), COUNT(*), AVG(ResolutionWidth) FROM hits
std::unique_ptr<IOperator> BuildQ3(const std::string& path) {
    return Global(Scan(path, {"AdvEngineID", "ResolutionWidth"}), Aggs(
                                                                      Sum(ColRef("AdvEngineID", I16), "sum_adv_engine_id", I64),
                                                                      CountStar("count"),
                                                                      Avg(ColRef("ResolutionWidth", I16), "avg_resolution_width", I64)));
}

// Q4: SELECT AVG(UserID) FROM hits
std::unique_ptr<IOperator> BuildQ4(const std::string& path) {
    return Global(Scan(path, {"UserID"}),
                  Aggs(Avg(ColRef("UserID", I64), "avg_user_id", I64)));
}

// Q5: SELECT COUNT(DISTINCT UserID) FROM hits
std::unique_ptr<IOperator> BuildQ5(const std::string& path) {
    return Global(Scan(path, {"UserID"}),
                  Aggs(CountDistinct(ColRef("UserID", I64), "count_distinct_user_id")));
}

// Q6: SELECT COUNT(DISTINCT SearchPhrase) FROM hits
std::unique_ptr<IOperator> BuildQ6(const std::string& path) {
    return Global(Scan(path, {"SearchPhrase"}),
                  Aggs(CountDistinct(ColRef("SearchPhrase", Str), "count_distinct_phrase")));
}

// Q7: SELECT MIN(EventDate), MAX(EventDate) FROM hits
std::unique_ptr<IOperator> BuildQ7(const std::string& path) {
    return Global(Scan(path, {"EventDate"}), Aggs(
                                                 Min(ColRef("EventDate", Date), "min_event_date"),
                                                 Max(ColRef("EventDate", Date), "max_event_date")));
}

// Q8: SELECT AdvEngineID, COUNT(*) FROM hits WHERE AdvEngineID <> 0
//     GROUP BY AdvEngineID ORDER BY COUNT(*) DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ8(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"AdvEngineID"}),
                  Cmp(ColRef("AdvEngineID", I16), CompareOp::NotEq, Literal(int16_t{0}, I16))),
            Keys(Key(ColRef("AdvEngineID", I16), "AdvEngineID")),
            Aggs(CountStar("count"))),
        SortBy(Desc(ColRef("count", I64))), 10);
}

// Q9: SELECT RegionID, COUNT(DISTINCT UserID) AS u FROM hits
//     GROUP BY RegionID ORDER BY u DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ9(const std::string& path) {
    return OrderLimit(
        GroupBy(Scan(path, {"RegionID", "UserID"}),
                Keys(Key(ColRef("RegionID", I32), "RegionID")),
                Aggs(CountDistinct(ColRef("UserID", I64), "u"))),
        SortBy(Desc(ColRef("u", I64))), 10);
}

// Q10: SELECT RegionID, SUM(AdvEngineID), COUNT(*) AS c, AVG(ResolutionWidth), COUNT(DISTINCT UserID)
//      FROM hits GROUP BY RegionID ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ10(const std::string& path) {
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

// Q11: SELECT MobilePhoneModel, COUNT(DISTINCT UserID) AS u FROM hits
//      WHERE MobilePhoneModel != '' GROUP BY MobilePhoneModel ORDER BY u DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ11(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"MobilePhoneModel", "UserID"}),
                  Cmp(ColRef("MobilePhoneModel", Str), CompareOp::NotEq, Literal(std::string{""}, Str))),
            Keys(Key(ColRef("MobilePhoneModel", Str), "MobilePhoneModel")),
            Aggs(CountDistinct(ColRef("UserID", I64), "u"))),
        SortBy(Desc(ColRef("u", I64))), 10);
}

// Q12: SELECT MobilePhone, MobilePhoneModel, COUNT(DISTINCT UserID) AS u FROM hits
//      WHERE MobilePhoneModel != '' GROUP BY MobilePhone, MobilePhoneModel ORDER BY u DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ12(const std::string& path) {
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

// Q13: SELECT SearchPhrase, COUNT(*) AS c FROM hits
//      WHERE SearchPhrase != '' GROUP BY SearchPhrase ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ13(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"SearchPhrase"}),
                  Cmp(ColRef("SearchPhrase", Str), CompareOp::NotEq, Literal(std::string{""}, Str))),
            Keys(Key(ColRef("SearchPhrase", Str), "SearchPhrase")),
            Aggs(CountStar("c"))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q14: SELECT SearchPhrase, COUNT(DISTINCT UserID) AS u FROM hits
//      WHERE SearchPhrase != '' GROUP BY SearchPhrase ORDER BY u DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ14(const std::string& path) {
    return OrderLimit(
        GroupBy(
            Where(Scan(path, {"SearchPhrase", "UserID"}),
                  Cmp(ColRef("SearchPhrase", Str), CompareOp::NotEq, Literal(std::string{""}, Str))),
            Keys(Key(ColRef("SearchPhrase", Str), "SearchPhrase")),
            Aggs(CountDistinct(ColRef("UserID", I64), "u"))),
        SortBy(Desc(ColRef("u", I64))), 10);
}

// Q15: SELECT SearchEngineID, SearchPhrase, COUNT(*) AS c FROM hits
//      WHERE SearchPhrase != '' GROUP BY SearchEngineID, SearchPhrase ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ15(const std::string& path) {
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

// Q16: SELECT UserID, COUNT(*) AS c FROM hits GROUP BY UserID ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ16(const std::string& path) {
    return OrderLimit(
        GroupBy(Scan(path, {"UserID"}),
                Keys(Key(ColRef("UserID", I64), "UserID")),
                Aggs(CountStar("c"))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q17: SELECT UserID, SearchPhrase, COUNT(*) AS c FROM hits
//      GROUP BY UserID, SearchPhrase ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ17(const std::string& path) {
    return OrderLimit(
        GroupBy(Scan(path, {"UserID", "SearchPhrase"}),
                Keys(
                    Key(ColRef("UserID", I64), "UserID"),
                    Key(ColRef("SearchPhrase", Str), "SearchPhrase")),
                Aggs(CountStar("c"))),
        SortBy(Desc(ColRef("c", I64))), 10);
}

// Q18: SELECT UserID, SearchPhrase, COUNT(*) AS c FROM hits
//      GROUP BY UserID, SearchPhrase ORDER BY c DESC LIMIT 10 OFFSET 1000
std::unique_ptr<IOperator> BuildQ18(const std::string& path) {
    return OrderLimit(
        GroupBy(Scan(path, {"UserID", "SearchPhrase"}),
                Keys(
                    Key(ColRef("UserID", I64), "UserID"),
                    Key(ColRef("SearchPhrase", Str), "SearchPhrase")),
                Aggs(CountStar("c"))),
        SortBy(Desc(ColRef("c", I64))), 10, 1000);
}

// Q19: SELECT UserID, toStartOfMinute(EventTime) AS m, SearchPhrase, COUNT(*) AS c, COUNT(DISTINCT UserID)
//      FROM hits GROUP BY UserID, m, SearchPhrase ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ19(const std::string& path) {
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

// Q20: SELECT UserID, ClientIP, COUNT(*) AS c, SUM(IsRefresh), AVG(ResolutionWidth)
//      FROM hits GROUP BY UserID, ClientIP ORDER BY c DESC LIMIT 10
std::unique_ptr<IOperator> BuildQ20(const std::string& path) {
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

}  // namespace Columnar::Exec
