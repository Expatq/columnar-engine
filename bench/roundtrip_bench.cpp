#include <io/csv/csv_writer.h>
#include <io/format/format_reader.h>
#include <parser/format/schema_parser.h>
#include <util/timer.h>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {

struct Options {
    std::filesystem::path csvPath;
    std::filesystem::path schemaPath;
    std::filesystem::path csv2iyxPath;
    std::filesystem::path workdir = ".";
    std::filesystem::path statsFile = "roundtrip_bench_stats.json";
    uint64_t syntheticRows = 200'000;
};

uint64_t ParseUnsigned(std::string_view value, std::string_view name) {
    if (value.empty())
        throw std::invalid_argument(std::string{name} + " cannot be empty");

    uint64_t result = 0;
    for (const char ch : value) {
        if (ch < '0' || ch > '9') {
            throw std::invalid_argument(std::string{name} + " must be an unsigned integer");
        }
        result = result * 10 + static_cast<uint64_t>(ch - '0');
    }
    return result;
}

void PrintUsage(const char* program) {
    std::cerr << "Usage: " << program
              << " [--csv FILE --schema FILE] [--csv2iyx PATH]"
                 " [--rows N] [--workdir DIR] [--stats FILE]\n"
              << "  Without --csv: generates synthetic data with --rows rows\n";
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
    throw std::runtime_error("cannot find csv2iyx binary; pass --csv2iyx PATH");
}

Options ParseArgs(int argc, char* argv[]) {
    Options options;
    options.csv2iyxPath = FindCsv2Iyx(argv[0]);

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto requireValue = [&](std::string_view flag) -> std::string {
            if (i + 1 >= argc)
                throw std::invalid_argument(std::string{flag} + " requires a value");
            return argv[++i];
        };

        if (arg == "--csv") {
            options.csvPath = requireValue(arg);
        } else if (arg == "--schema") {
            options.schemaPath = requireValue(arg);
        } else if (arg == "--csv2iyx") {
            options.csv2iyxPath = requireValue(arg);
        } else if (arg == "--rows") {
            options.syntheticRows = ParseUnsigned(requireValue(arg), "rows");
        } else if (arg == "--workdir") {
            options.workdir = requireValue(arg);
        } else if (arg == "--stats") {
            options.statsFile = requireValue(arg);
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            std::exit(0);
        } else {
            throw std::invalid_argument("unknown argument: " + arg);
        }
    }

    if (options.csvPath.empty() != options.schemaPath.empty())
        throw std::invalid_argument("--csv and --schema must be given together");

    if (options.syntheticRows == 0)
        throw std::invalid_argument("--rows must be positive");

    return options;
}

uint64_t FileSize(const std::filesystem::path& path) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    return ec ? 0 : static_cast<uint64_t>(size);
}

void WriteSyntheticInput(const std::filesystem::path& dataFile,
                         const std::filesystem::path& schemaFile,
                         uint64_t rows) {
    {
        std::ofstream schema(schemaFile);
        schema << "id,int64\n"
               << "value,int32\n"
               << "enabled,bool\n"
               << "payload,string\n";
    }

    std::ofstream data(dataFile);
    for (uint64_t row = 0; row < rows; ++row) {
        data << row << ',' << (row * 17) % 1'000'000 << ','
             << (row % 2 == 0 ? "true" : "false") << ','
             << "payload_" << row << "_abcdefghijklmnopqrstuvwxyz\n";
    }
}

struct BenchResult {
    uint64_t rows = 0;
    uint64_t inputCsvBytes = 0;
    uint64_t iyxBytes = 0;
    uint64_t outputCsvBytes = 0;
    size_t rowGroups = 0;
    double csvToIyxSeconds = 0.0;
    double iyxToCsvSeconds = 0.0;
    double totalSeconds = 0.0;
};

BenchResult RunRoundTrip(const Options& options) {
    const std::filesystem::path workdir(options.workdir);
    std::filesystem::create_directories(workdir);

    std::filesystem::path csvFile = options.csvPath;
    std::filesystem::path schemaFile = options.schemaPath;
    const auto iyxFile = workdir / "bench_data.iyx";
    const auto outputCsvFile = workdir / "bench_output.csv";

    if (csvFile.empty()) {
        csvFile = workdir / "bench_input.csv";
        schemaFile = workdir / "bench_schema.csv";
        WriteSyntheticInput(csvFile, schemaFile, options.syntheticRows);
    }

    BenchResult result;
    result.inputCsvBytes = FileSize(csvFile);

    Columnar::Util::Timer totalTimer;

    {
        const std::string cmd =
            options.csv2iyxPath.string() + " " +
            schemaFile.string() + " " +
            csvFile.string() + " " +
            iyxFile.string();

        Columnar::Util::Timer timer;
        const int ret = std::system(cmd.c_str());
        if (ret != 0)
            throw std::runtime_error("csv2iyx exited with code " + std::to_string(ret));
        result.csvToIyxSeconds = timer.ElapsedSeconds();

        Columnar::IO::FormatReader reader(iyxFile.string());
        result.rows = reader.GetTotalRowCount();
        result.rowGroups = reader.GetRowGroupCount();
    }

    result.iyxBytes = FileSize(iyxFile);

    {
        Columnar::Util::Timer timer;
        Columnar::IO::FormatReader formatReader(iyxFile.string());
        Columnar::IO::CsvWriter csvWriter(outputCsvFile.string());

        while (auto rg = formatReader.ReadRowGroup()) {
            csvWriter.WriteRowGroup(*rg);
        }

        csvWriter.Flush();
        result.iyxToCsvSeconds = timer.ElapsedSeconds();
    }

    result.totalSeconds = totalTimer.ElapsedSeconds();
    result.outputCsvBytes = FileSize(outputCsvFile);
    return result;
}

double RowsPerSecond(uint64_t rows, double seconds) {
    return seconds > 0.0 ? static_cast<double>(rows) / seconds : 0.0;
}

void DumpStats(const std::filesystem::path& statsFile, const BenchResult& result) {
    std::ofstream output(statsFile);
    output << "{\n"
           << "  \"rows\": " << result.rows << ",\n"
           << "  \"input_csv_bytes\": " << result.inputCsvBytes << ",\n"
           << "  \"iyx_bytes\": " << result.iyxBytes << ",\n"
           << "  \"output_csv_bytes\": " << result.outputCsvBytes << ",\n"
           << "  \"row_groups\": " << result.rowGroups << ",\n"
           << "  \"csv_to_iyx_seconds\": " << result.csvToIyxSeconds << ",\n"
           << "  \"iyx_to_csv_seconds\": " << result.iyxToCsvSeconds << ",\n"
           << "  \"total_seconds\": " << result.totalSeconds << ",\n"
           << "  \"csv_to_iyx_rows_per_second\": "
           << RowsPerSecond(result.rows, result.csvToIyxSeconds) << ",\n"
           << "  \"iyx_to_csv_rows_per_second\": "
           << RowsPerSecond(result.rows, result.iyxToCsvSeconds) << "\n"
           << "}\n";
}

void PrintResult(const BenchResult& result, const std::filesystem::path& statsFile) {
    std::cout << "RoundTripCsvIyxCsv benchmark\n"
              << "  rows:       " << result.rows << "\n"
              << "  input csv:  " << result.inputCsvBytes << " bytes\n"
              << "  iyx:        " << result.iyxBytes << " bytes\n"
              << "  output csv: " << result.outputCsvBytes << " bytes\n"
              << "  row groups: " << result.rowGroups << "\n"
              << "  csv->iyx:   "
              << Columnar::Util::FormatSeconds(result.csvToIyxSeconds)
              << " (" << Columnar::Util::FormatRowsPerSecond(result.rows, result.csvToIyxSeconds)
              << ")\n"
              << "  iyx->csv:   "
              << Columnar::Util::FormatSeconds(result.iyxToCsvSeconds)
              << " (" << Columnar::Util::FormatRowsPerSecond(result.rows, result.iyxToCsvSeconds)
              << ")\n"
              << "  total:      "
              << Columnar::Util::FormatSeconds(result.totalSeconds) << "\n"
              << "  stats:      " << statsFile << "\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const Options options = ParseArgs(argc, argv);
        const BenchResult result = RunRoundTrip(options);
        const std::filesystem::path statsFile =
            std::filesystem::path(options.workdir) / options.statsFile;

        DumpStats(statsFile, result);
        PrintResult(result, statsFile);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        PrintUsage(argv[0]);
        return 1;
    }

    return 0;
}
