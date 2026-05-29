#include <exec/core/exec_batch.h>
#include <exec/core/operator_runner.h>
#include <exec/query/clickbench_queries.h>

#include <io/format/format_reader.h>

#include <parser/format/serialize_to_string.h>

#include <util/timer.h>

#include <unistd.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

struct Options {
    std::filesystem::path csvPath;
    std::filesystem::path schemaPath = "script/hits.schema";
    std::filesystem::path iyxPath;
    std::filesystem::path csv2iyxPath;
    std::filesystem::path timingsPath = "clickbench_timings.csv";
    std::filesystem::path conversionStatsPath =
        "clickbench_conversion_stats.csv";
    std::filesystem::path queryStatsPath = "clickbench_query_stats.csv";
    std::filesystem::path answersDir;
    std::filesystem::path dumpAnswersDir;
    size_t runs = 1;
    bool reuseIyx = false;
    bool continueOnError = false;
};

struct PreparedCsv {
    std::filesystem::path path;
};

struct ConversionStats {
    bool converted = false;
    uint64_t rows = 0;
    uint64_t inputCsvBytes = 0;
    uint64_t outputIyxBytes = 0;
    size_t rowGroups = 0;
    int64_t elapsedMs = 0;
    double elapsedSeconds = 0.0;
};

struct QueryRunStats {
    size_t queryId = 0;
    size_t run = 0;
    bool ok = false;
    int64_t elapsedMs = 0;
    size_t resultBatches = 0;
    size_t resultRows = 0;
    std::string error;
    bool validationSkipped = false;
    bool validationOk = false;
    bool validationAmbiguous = false;
    std::string validationError;
};

[[noreturn]] void Usage(const char* program) {
    std::cerr
        << "Usage: " << program << " --csv RELATIVE_PATH "
        << "[--schema FILE] [--iyx FILE] [--csv2iyx PATH] [--answers DIR] "
        << "[--dump-answers DIR] [--timings FILE] [--conversion-stats FILE] "
        << "[--query-stats FILE] [--runs N] [--reuse-iyx] [--continue-on-error]\n";
    std::exit(EXIT_FAILURE);
}

std::string RequireValue(int argc, char** argv, int& index) {
    if (index + 1 >= argc) {
        throw std::invalid_argument(std::string{argv[index]} +
                                    " requires a value");
    }
    return argv[++index];
}

uint64_t ParsePositiveUInt(std::string_view value, std::string_view name) {
    if (value.empty()) {
        throw std::invalid_argument(std::string{name} + " cannot be empty");
    }

    uint64_t result = 0;
    for (const char ch : value) {
        if (ch < '0' || ch > '9') {
            throw std::invalid_argument(std::string{name} +
                                        " must be a positive integer");
        }
        const uint64_t digit = static_cast<uint64_t>(ch - '0');
        if (result > (std::numeric_limits<uint64_t>::max() - digit) / 10) {
            throw std::overflow_error(std::string{name} + " is too large");
        }
        result = result * 10 + digit;
    }
    if (result == 0) {
        throw std::invalid_argument(std::string{name} + " must be positive");
    }
    return result;
}

std::filesystem::path FindCsv2Iyx(const char* argv0) {
    std::error_code ec;
    const auto self = std::filesystem::canonical(argv0, ec);
    if (!ec) {
        auto candidate = self.parent_path() / "csv2iyx";
        if (std::filesystem::exists(candidate))
            return candidate;
    }
    const std::filesystem::path fallback = "tools/csv2iyx/csv2iyx";
    if (std::filesystem::exists(fallback))
        return fallback;
    return {};
}

Options ParseArgs(int argc, char** argv) {
    Options options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--csv") {
            options.csvPath = RequireValue(argc, argv, i);
        } else if (arg == "--schema") {
            options.schemaPath = RequireValue(argc, argv, i);
        } else if (arg == "--iyx") {
            options.iyxPath = RequireValue(argc, argv, i);
        } else if (arg == "--csv2iyx") {
            options.csv2iyxPath = RequireValue(argc, argv, i);
        } else if (arg == "--answers") {
            options.answersDir = RequireValue(argc, argv, i);
        } else if (arg == "--dump-answers") {
            options.dumpAnswersDir = RequireValue(argc, argv, i);
        } else if (arg == "--timings") {
            options.timingsPath = RequireValue(argc, argv, i);
        } else if (arg == "--conversion-stats") {
            options.conversionStatsPath = RequireValue(argc, argv, i);
        } else if (arg == "--query-stats") {
            options.queryStatsPath = RequireValue(argc, argv, i);
        } else if (arg == "--runs") {
            options.runs = static_cast<size_t>(
                ParsePositiveUInt(RequireValue(argc, argv, i), "runs"));
        } else if (arg == "--reuse-iyx") {
            options.reuseIyx = true;
        } else if (arg == "--continue-on-error") {
            options.continueOnError = true;
        } else if (arg == "--help" || arg == "-h") {
            Usage(argv[0]);
        } else {
            throw std::invalid_argument("unknown argument: " + arg);
        }
    }

    if (options.csvPath.empty()) {
        throw std::invalid_argument("--csv is required");
    }

    if (options.iyxPath.empty()) {
        options.iyxPath = options.csvPath;
        options.iyxPath.replace_extension(".iyx");
    }

    if (options.csv2iyxPath.empty())
        options.csv2iyxPath = FindCsv2Iyx(argv[0]);
    if (options.csv2iyxPath.empty())
        throw std::runtime_error("cannot find csv2iyx binary; pass --csv2iyx PATH");

    return options;
}

uint64_t FileSize(const std::filesystem::path& path) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    return ec ? 0 : static_cast<uint64_t>(size);
}

void EnsureParentDirectory(const std::filesystem::path& path) {
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

PreparedCsv RequireExistingCsv(const std::filesystem::path& path) {
    if (path.empty()) {
        throw std::invalid_argument("CSV path must not be empty");
    }

    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("CSV file does not exist: " +
                                 path.string());
    }
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error("CSV path is not a regular file: " +
                                 path.string());
    }
    if (FileSize(path) == 0) {
        throw std::runtime_error("CSV file is empty: " +
                                 path.string());
    }

    return {.path = path};
}

ConversionStats ConvertCsvToIyx(const std::filesystem::path& csv2iyxPath,
                                const std::filesystem::path& schemaPath,
                                const std::filesystem::path& csvPath,
                                const std::filesystem::path& iyxPath,
                                bool reuseIyx) {
    ConversionStats stats;
    stats.inputCsvBytes = FileSize(csvPath);

    if (reuseIyx && std::filesystem::exists(iyxPath)) {
        stats.outputIyxBytes = FileSize(iyxPath);
        return stats;
    }

    EnsureParentDirectory(iyxPath);
    const auto tmpPath = iyxPath.string() + ".tmp." + std::to_string(::getpid());
    std::filesystem::remove(tmpPath);

    const std::string cmd =
        csv2iyxPath.string() + " " +
        schemaPath.string() + " " +
        csvPath.string() + " " +
        tmpPath;

    Columnar::Util::Timer timer;
    try {
        const int ret = std::system(cmd.c_str());
        if (ret != 0)
            throw std::runtime_error("csv2iyx exited with code " + std::to_string(ret));

        stats.elapsedMs = timer.ElapsedMilliseconds();
        stats.elapsedSeconds = timer.ElapsedSeconds();

        std::filesystem::rename(tmpPath, iyxPath);

        Columnar::IO::FormatReader reader(iyxPath.string());
        stats.rows = reader.GetTotalRowCount();
        stats.rowGroups = reader.GetRowGroupCount();
    } catch (...) {
        std::filesystem::remove(tmpPath);
        throw;
    }

    stats.converted = true;
    stats.outputIyxBytes = FileSize(iyxPath);
    return stats;
}

std::optional<std::vector<std::string>> LoadReference(
    const std::filesystem::path& answersDir, size_t queryId) {
    char name[16];
    std::snprintf(name, sizeof(name), "query_%02zu.csv", queryId);

    std::ifstream f(answersDir / name);
    if (!f.is_open())
        return std::nullopt;

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty())
            lines.push_back(std::move(line));
    }
    return lines;
}

void DumpCollected(const std::filesystem::path& dir, size_t queryId,
                   const std::vector<std::string>& collected) {
    std::filesystem::create_directories(dir);
    char name[16];
    std::snprintf(name, sizeof(name), "query_%02zu.csv", queryId);
    std::ofstream f(dir / name);
    for (const auto& line : collected)
        f << line << '\n';
}

std::string SerialiseRow(const Columnar::Exec::ExecBatch& batch, size_t rid) {
    const Columnar::RowGroup& rg = *batch.rowGroup;
    const Columnar::Schema& schema = rg.GetSchema();
    const size_t ncols = rg.GetColumnCount();

    std::string line;
    for (size_t c = 0; c < ncols; ++c) {
        if (c > 0)
            line += ',';
        line += Columnar::Parser::FormatColumn(
            rg.GetColumn(c), rid, schema.GetColumn(c).logical);
    }
    return line;
}

struct ValidateResult {
    bool ok = false;
    bool ambiguous = false;
    std::string error;
};

ValidateResult Validate(const std::vector<std::string>& collected,
                        const std::vector<std::string>& reference) {
    if (collected.size() != reference.size()) {
        return {.error = "row count: got " + std::to_string(collected.size()) +
                         ", expected " + std::to_string(reference.size())};
    }

    bool exact = true;
    for (size_t i = 0; i < collected.size(); ++i) {
        if (collected[i] != reference[i]) {
            exact = false;
            break;
        }
    }
    if (exact)
        return {.ok = true};

    auto sc = collected;
    auto sr = reference;
    std::sort(sc.begin(), sc.end());
    std::sort(sr.begin(), sr.end());
    if (sc == sr)
        return {.ambiguous = true};

    for (size_t i = 0; i < collected.size(); ++i) {
        if (collected[i] != reference[i]) {
            return {.error = "row " + std::to_string(i) + ":\n  got:      " +
                             collected[i] + "\n  expected: " + reference[i]};
        }
    }
    return {};  // unreachable
}

QueryRunStats RunQueryOnce(const std::filesystem::path& iyxPath,
                           size_t queryId, size_t run,
                           const std::filesystem::path& answersDir,
                           const std::filesystem::path& dumpAnswersDir) {
    QueryRunStats stats;
    stats.queryId = queryId;
    stats.run = run;

    auto root = Columnar::Exec::BuildQuery(iyxPath.string(), queryId);
    Columnar::Exec::OperatorRunner runner(*root);

    std::vector<std::string> collected;
    const bool doValidate = !answersDir.empty();
    const bool doDump = !dumpAnswersDir.empty() && run == 0;

    Columnar::Util::Timer timer;
    try {
        runner.Open();

        Columnar::Exec::ExecBatch batch;
        while (runner.Next(batch)) {
            ++stats.resultBatches;
            stats.resultRows += batch.ActiveRowCount();

            if (doValidate || doDump) {
                if (batch.has_selection) {
                    for (const auto rid : batch.selection.Rows())
                        collected.push_back(SerialiseRow(batch, rid));
                } else {
                    for (size_t rid = 0; rid < batch.rowCount; ++rid)
                        collected.push_back(SerialiseRow(batch, rid));
                }
            }
        }

        runner.Close();
        stats.elapsedMs = timer.ElapsedMilliseconds();
        stats.ok = true;
    } catch (const std::exception& e) {
        runner.Close();
        stats.elapsedMs = timer.ElapsedMilliseconds();
        stats.error = e.what();
    }

    if (stats.ok && doDump)
        DumpCollected(dumpAnswersDir, queryId, collected);

    if (stats.ok && doValidate) {
        auto ref = LoadReference(answersDir, queryId);
        if (!ref) {
            stats.validationSkipped = true;
        } else {
            auto res = Validate(collected, *ref);
            stats.validationOk = res.ok;
            stats.validationAmbiguous = res.ambiguous;
            stats.validationError = res.error;
        }
    }

    return stats;
}

int64_t MedianTimeMs(std::vector<QueryRunStats> runs) {
    runs.erase(std::remove_if(runs.begin(), runs.end(),
                              [](const QueryRunStats& s) { return !s.ok; }),
               runs.end());
    if (runs.empty()) {
        throw std::runtime_error("cannot calculate median for failed query");
    }

    const auto mid = runs.begin() + static_cast<std::ptrdiff_t>(runs.size() / 2);
    std::nth_element(runs.begin(), mid, runs.end(),
                     [](const QueryRunStats& lhs, const QueryRunStats& rhs) {
                         return lhs.elapsedMs < rhs.elapsedMs;
                     });
    return mid->elapsedMs;
}

double RowsPerSecond(uint64_t rows, double seconds) {
    return seconds > 0.0 ? static_cast<double>(rows) / seconds : 0.0;
}

double MibPerSecond(uint64_t bytes, double seconds) {
    constexpr double kMib = 1024.0 * 1024.0;
    return seconds > 0.0 ? static_cast<double>(bytes) / kMib / seconds : 0.0;
}

void WriteConversionStats(const std::filesystem::path& path,
                          const ConversionStats& stats) {
    EnsureParentDirectory(path);
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("cannot write conversion stats: " +
                                 path.string());
    }

    const double compressionRatio =
        stats.outputIyxBytes == 0
            ? 0.0
            : static_cast<double>(stats.inputCsvBytes) /
                  static_cast<double>(stats.outputIyxBytes);

    out << "converted,rows,input_csv_bytes,output_iyx_bytes,row_groups,"
           "elapsed_ms,rows_per_second,input_mib_per_second,compression_ratio\n";
    out << (stats.converted ? 1 : 0) << ','
        << stats.rows << ','
        << stats.inputCsvBytes << ','
        << stats.outputIyxBytes << ','
        << stats.rowGroups << ','
        << stats.elapsedMs << ','
        << RowsPerSecond(stats.rows, stats.elapsedSeconds) << ','
        << MibPerSecond(stats.inputCsvBytes, stats.elapsedSeconds) << ','
        << compressionRatio << '\n';
}

std::string_view ValidationLabel(const QueryRunStats& s) {
    if (s.validationSkipped)
        return "skip";
    if (s.validationOk)
        return "ok";
    if (s.validationAmbiguous)
        return "ambiguous";
    return "fail";
}

void WriteQueryStats(const std::filesystem::path& path,
                     const std::vector<QueryRunStats>& stats) {
    EnsureParentDirectory(path);
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("cannot write query stats: " + path.string());
    }

    out << "query,run,status,elapsed_ms,result_batches,result_rows,error,"
           "validation,validation_error\n";
    for (const auto& s : stats) {
        out << 'q' << s.queryId << ','
            << s.run << ','
            << (s.ok ? "ok" : "error") << ','
            << s.elapsedMs << ','
            << s.resultBatches << ','
            << s.resultRows << ',';
        for (const char ch : s.error) {
            out << (ch == ',' || ch == '\n' || ch == '\r' ? ' ' : ch);
        }
        out << ',' << ValidationLabel(s) << ',';
        for (const char ch : s.validationError) {
            out << (ch == ',' || ch == '\n' || ch == '\r' ? ' ' : ch);
        }
        out << '\n';
    }
}

void WriteMainTimings(const std::filesystem::path& path,
                      const std::vector<int64_t>& medianTimesMs) {
    EnsureParentDirectory(path);
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("cannot write timings: " + path.string());
    }

    for (size_t queryId = 0; queryId < medianTimesMs.size(); ++queryId) {
        out << 'q' << queryId << ',' << medianTimesMs[queryId] << '\n';
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options options = ParseArgs(argc, argv);
        const PreparedCsv csv = RequireExistingCsv(options.csvPath);

        const ConversionStats conversion =
            ConvertCsvToIyx(options.csv2iyxPath, options.schemaPath,
                            csv.path, options.iyxPath, options.reuseIyx);
        WriteConversionStats(options.conversionStatsPath, conversion);

        std::vector<QueryRunStats> allRuns;
        std::vector<int64_t> medianTimesMs;
        allRuns.reserve(Columnar::Exec::kClickBenchQueryCount * options.runs);
        medianTimesMs.reserve(Columnar::Exec::kClickBenchQueryCount);

        size_t validationOkCount = 0;
        size_t validationAmbiguousCount = 0;
        size_t validationFailCount = 0;
        size_t validationSkipCount = 0;
        const bool doValidate = !options.answersDir.empty();

        for (size_t queryId = 0; queryId < Columnar::Exec::kClickBenchQueryCount; ++queryId) {
            std::vector<QueryRunStats> queryRuns;
            queryRuns.reserve(options.runs);

            for (size_t run = 0; run < options.runs; ++run) {
                QueryRunStats stats = RunQueryOnce(
                    options.iyxPath, queryId, run,
                    options.answersDir, options.dumpAnswersDir);
                if (!stats.ok && !options.continueOnError) {
                    throw std::runtime_error("q" + std::to_string(queryId) +
                                             " failed: " + stats.error);
                }
                queryRuns.push_back(stats);
                allRuns.push_back(queryRuns.back());
            }

            if (std::any_of(queryRuns.begin(), queryRuns.end(),
                            [](const QueryRunStats& s) { return s.ok; })) {
                medianTimesMs.push_back(MedianTimeMs(queryRuns));
            } else {
                medianTimesMs.push_back(-1);
            }

            const QueryRunStats& first = queryRuns.front();
            std::cout << 'q' << queryId << ": " << medianTimesMs.back() << " ms";
            if (doValidate) {
                if (first.validationSkipped) {
                    std::cout << "  [SKIP]";
                    ++validationSkipCount;
                } else if (first.validationOk) {
                    std::cout << "  [OK]";
                    ++validationOkCount;
                } else if (first.validationAmbiguous) {
                    std::cout << "  [AMBIGUOUS] row order differs from reference";
                    ++validationAmbiguousCount;
                } else {
                    std::cout << "  [FAIL] " << first.validationError;
                    ++validationFailCount;
                }
            }
            std::cout << '\n'
                      << std::flush;
        }

        if (doValidate) {
            std::cout << "Validation: "
                      << validationOkCount << " ok, "
                      << validationAmbiguousCount << " ambiguous, "
                      << validationFailCount << " failed, "
                      << validationSkipCount << " skipped\n";
        }
        if (!options.dumpAnswersDir.empty()) {
            std::cout << "  answers dumped to: " << options.dumpAnswersDir << '\n';
        }

        WriteMainTimings(options.timingsPath, medianTimesMs);
        WriteQueryStats(options.queryStatsPath, allRuns);

        std::cout << "ClickBench benchmark finished\n"
                  << "  csv: " << csv.path << '\n'
                  << "  iyx: " << options.iyxPath << '\n'
                  << "  conversion: " << options.conversionStatsPath << '\n'
                  << "  timings: " << options.timingsPath << '\n'
                  << "  query stats: " << options.queryStatsPath << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        Usage(argv[0]);
    }
}
