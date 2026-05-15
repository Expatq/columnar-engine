#include <exec/core/exec_batch.h>
#include <exec/core/operator_runner.h>
#include <exec/query/clickbench_queries.h>

#include <io/csv/csv_reader.h>
#include <io/format/format_writer.h>

#include <parser/format/schema_parser.h>

#include <util/timer.h>

#include <unistd.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
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
    std::filesystem::path timingsPath = "clickbench_timings.csv";
    std::filesystem::path conversionStatsPath =
        "clickbench_conversion_stats.csv";
    std::filesystem::path queryStatsPath = "clickbench_query_stats.csv";
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
};

[[noreturn]] void Usage(const char* program) {
    std::cerr
        << "Usage: " << program << " --csv RELATIVE_PATH "
        << "[--schema FILE] [--iyx FILE] [--timings FILE] "
        << "[--conversion-stats FILE] [--query-stats FILE] [--runs N] "
        << "[--reuse-iyx] [--continue-on-error]\n";
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
    if (options.csvPath.is_absolute()) {
        throw std::invalid_argument("--csv must be a relative path");
    }
    for (const auto& part : options.csvPath) {
        if (part == "..") {
            throw std::invalid_argument("--csv must not contain '..'");
        }
    }

    if (options.iyxPath.empty()) {
        options.iyxPath = options.csvPath;
        options.iyxPath.replace_extension(".iyx");
    }

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

PreparedCsv RequireExistingCsv(const std::filesystem::path& relativePath) {
    if (relativePath.empty() || relativePath.is_absolute()) {
        throw std::invalid_argument("CSV path must be relative");
    }
    for (const auto& part : relativePath) {
        if (part == "..") {
            throw std::invalid_argument("CSV path must not contain '..'");
        }
    }

    if (!std::filesystem::exists(relativePath)) {
        throw std::runtime_error("CSV file does not exist: " +
                                 relativePath.string());
    }
    if (!std::filesystem::is_regular_file(relativePath)) {
        throw std::runtime_error("CSV path is not a regular file: " +
                                 relativePath.string());
    }
    if (FileSize(relativePath) == 0) {
        throw std::runtime_error("CSV file is empty: " +
                                 relativePath.string());
    }

    return {.path = relativePath};
}

ConversionStats ConvertCsvToIyx(const std::filesystem::path& schemaPath,
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

    const Columnar::Schema schema =
        Columnar::Parser::LoadSchemaFromCsv(schemaPath.string());

    Columnar::Util::Timer timer;
    try {
        Columnar::IO::CsvReader csvReader(csvPath.string(), schema);
        Columnar::IO::FormatWriter formatWriter(tmpPath);
        formatWriter.Begin(schema);

        while (auto rowGroup = csvReader.ReadRowGroup()) {
            formatWriter.WriteRowGroup(*rowGroup);
        }

        formatWriter.End();
        stats.elapsedMs = timer.ElapsedMilliseconds();
        stats.elapsedSeconds = timer.ElapsedSeconds();
        stats.rows = csvReader.GetTotalRowsRead();
        stats.rowGroups = formatWriter.GetRowGroupCount();

        std::filesystem::rename(tmpPath, iyxPath);
    } catch (...) {
        std::filesystem::remove(tmpPath);
        throw;
    }

    stats.converted = true;
    stats.outputIyxBytes = FileSize(iyxPath);
    return stats;
}

QueryRunStats RunQueryOnce(const std::filesystem::path& iyxPath, size_t queryId, size_t run) {
    QueryRunStats stats;
    stats.queryId = queryId;
    stats.run = run;

    auto root = Columnar::Exec::BuildQuery(iyxPath.string(), queryId);
    Columnar::Exec::OperatorRunner runner(*root);

    Columnar::Util::Timer timer;
    try {
        runner.Open();

        Columnar::Exec::ExecBatch batch;
        while (runner.Next(batch)) {
            ++stats.resultBatches;
            stats.resultRows += batch.ActiveRowCount();
        }

        runner.Close();
        stats.elapsedMs = timer.ElapsedMilliseconds();
        stats.ok = true;
    } catch (const std::exception& e) {
        runner.Close();
        stats.elapsedMs = timer.ElapsedMilliseconds();
        stats.error = e.what();
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

void WriteQueryStats(const std::filesystem::path& path,
                     const std::vector<QueryRunStats>& stats) {
    EnsureParentDirectory(path);
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("cannot write query stats: " + path.string());
    }

    out << "query,run,status,elapsed_ms,result_batches,result_rows,error\n";
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
            ConvertCsvToIyx(options.schemaPath, csv.path, options.iyxPath,
                            options.reuseIyx);
        WriteConversionStats(options.conversionStatsPath, conversion);

        std::vector<QueryRunStats> allRuns;
        std::vector<int64_t> medianTimesMs;
        allRuns.reserve(Columnar::Exec::kClickBenchQueryCount * options.runs);
        medianTimesMs.reserve(Columnar::Exec::kClickBenchQueryCount);

        for (size_t queryId = 0; queryId < Columnar::Exec::kClickBenchQueryCount; ++queryId) {
            std::vector<QueryRunStats> queryRuns;
            queryRuns.reserve(options.runs);

            for (size_t run = 0; run < options.runs; ++run) {
                QueryRunStats stats =
                    RunQueryOnce(options.iyxPath, queryId, run);
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

            std::cout << 'q' << queryId << ": " << medianTimesMs.back()
                      << " ms\n"
                      << std::flush;
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
