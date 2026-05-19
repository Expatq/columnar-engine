#include <io/csv/csv_reader.h>
#include <io/csv/parallel/csv_boundary_index.h>
#include <io/csv/parallel/csv_range_reader.h>
#include <io/format/format_merger.h>
#include <io/format/format_writer.h>
#include <parser/format/schema_parser.h>
#include <util/timer.h>

#include <atomic>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Options {
    std::filesystem::path schemaPath;
    std::filesystem::path inputPath;
    std::filesystem::path outputPath;
    std::filesystem::path tmpDir;
    size_t threads = 1;
    size_t rowGroupSize = 65536;
};

struct PartResult {
    std::filesystem::path path;
    uint64_t rows = 0;
    uint64_t rowGroups = 0;
};

[[noreturn]] void Usage(const char* program) {
    std::cerr
        << "Usage:\n"
        << "  " << program << " <schema.csv> <data.csv> <output.iyx>\n"
        << "  " << program
        << " --schema FILE --input FILE --output FILE "
        << "[--threads N] [--tmp-dir DIR] [--row-group-size N]\n";
    std::exit(EXIT_FAILURE);
}

std::string RequireValue(int argc, char** argv, int& index) {
    if (index + 1 >= argc) {
        throw std::invalid_argument(std::string{argv[index]} + " requires a value");
    }
    return argv[++index];
}

size_t ParsePositiveSize(const std::string& value, const char* name) {
    if (value.empty()) {
        throw std::invalid_argument(std::string{name} + " cannot be empty");
    }

    size_t result = 0;
    for (char ch : value) {
        if (ch < '0' || ch > '9') {
            throw std::invalid_argument(std::string{name} + " must be numeric");
        }
        result = result * 10 + static_cast<size_t>(ch - '0');
    }

    if (result == 0) {
        throw std::invalid_argument(std::string{name} + " must be positive");
    }
    return result;
}

Options ParseArgs(int argc, char** argv) {
    Options options;

    if (argc == 4 && argv[1][0] != '-') {
        options.schemaPath = argv[1];
        options.inputPath = argv[2];
        options.outputPath = argv[3];
        options.threads = 1;
        return options;
    }

    const unsigned hw = std::thread::hardware_concurrency();
    options.threads = hw == 0 ? 4 : static_cast<size_t>(hw);

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--schema") {
            options.schemaPath = RequireValue(argc, argv, i);
        } else if (arg == "--input") {
            options.inputPath = RequireValue(argc, argv, i);
        } else if (arg == "--output") {
            options.outputPath = RequireValue(argc, argv, i);
        } else if (arg == "--threads") {
            options.threads = ParsePositiveSize(RequireValue(argc, argv, i), "threads");
        } else if (arg == "--tmp-dir") {
            options.tmpDir = RequireValue(argc, argv, i);
        } else if (arg == "--row-group-size") {
            options.rowGroupSize =
                ParsePositiveSize(RequireValue(argc, argv, i), "row-group-size");
        } else if (arg == "--help" || arg == "-h") {
            Usage(argv[0]);
        } else {
            throw std::invalid_argument("unknown argument: " + arg);
        }
    }

    if (options.schemaPath.empty() ||
        options.inputPath.empty() ||
        options.outputPath.empty()) {
        Usage(argv[0]);
    }

    if (options.tmpDir.empty()) {
        options.tmpDir = options.outputPath.string() + ".parts";
    }

    return options;
}

void EnsureParentDirectory(const std::filesystem::path& path) {
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

void ConvertSequential(const Options& options, const Columnar::Schema& schema) {
    Columnar::IO::CsvReader reader(options.inputPath.string(), schema);
    Columnar::IO::FormatWriter writer(options.outputPath.string());

    writer.Begin(schema);

    size_t rowGroupNum = 0;
    while (auto rg = reader.ReadRowGroup()) {
        writer.WriteRowGroup(*rg);
        ++rowGroupNum;
    }

    writer.End();

    std::cerr << "Done! Total: " << reader.GetTotalRowsRead() << " rows, "
              << rowGroupNum << " row groups\n";
}

PartResult ConvertPart(const Options& options,
                       const Columnar::Schema& schema,
                       const Columnar::IO::CsvBoundaryIndexer::RecordRange& range,
                       size_t partIdx) {
    const auto tmpPath =
        options.tmpDir / ("part_" + std::to_string(partIdx) + ".iyx.tmp");
    const auto partPath =
        options.tmpDir / ("part_" + std::to_string(partIdx) + ".iyx");

    std::filesystem::remove(tmpPath);
    std::filesystem::remove(partPath);

    Columnar::IO::CsvRangeReader reader(
        options.inputPath,
        schema,
        range,
        options.rowGroupSize);
    Columnar::IO::FormatWriter writer(tmpPath.string());

    writer.Begin(schema);
    while (auto rg = reader.ReadRowGroup()) {
        writer.WriteRowGroup(*rg);
    }
    writer.End();

    std::filesystem::rename(tmpPath, partPath);

    return PartResult{
        .path = partPath,
        .rows = reader.GetRowsRead(),
        .rowGroups = writer.GetRowGroupCount(),
    };
}

void ConvertParallel(const Options& options, const Columnar::Schema& schema) {
    std::filesystem::remove_all(options.tmpDir);
    std::filesystem::create_directories(options.tmpDir);

    Columnar::IO::CsvBoundaryIndexer indexer;
    Columnar::IO::CsvBoundaryIndexer::Options indexOptions;
    indexOptions.threads = options.threads;
    indexOptions.targetPartitions = options.threads * 4;

    Columnar::Util::Timer timer;
    const auto plan = indexer.BuildPlan(options.inputPath, indexOptions);
    std::cerr << "Indexed: " << plan.recordCount << " records, "
              << plan.ranges.size() << " partitions in "
              << Columnar::Util::FormatSeconds(timer.ElapsedSeconds()) << '\n';

    std::vector<PartResult> results(plan.ranges.size());
    std::atomic<size_t> nextRange = 0;
    std::mutex errorMutex;
    std::exception_ptr firstError;

    auto saveError = [&](std::exception_ptr error) {
        std::lock_guard guard(errorMutex);
        if (!firstError) {
            firstError = error;
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(options.threads);

    timer.Reset();
    for (size_t i = 0; i < options.threads; ++i) {
        workers.emplace_back([&] {
            try {
                while (true) {
                    const size_t rangeIdx = nextRange.fetch_add(1, std::memory_order_relaxed);
                    if (rangeIdx >= plan.ranges.size()) {
                        return;
                    }
                    results[rangeIdx] =
                        ConvertPart(options, schema, plan.ranges[rangeIdx], rangeIdx);
                }
            } catch (...) {
                saveError(std::current_exception());
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    if (firstError) {
        std::rethrow_exception(firstError);
    }

    std::cerr << "Converted parts in "
              << Columnar::Util::FormatSeconds(timer.ElapsedSeconds()) << '\n';

    std::vector<Columnar::IO::MergeInput> inputs;
    inputs.reserve(results.size());

    uint64_t rows = 0;
    uint64_t rowGroups = 0;
    for (const auto& result : results) {
        inputs.push_back(Columnar::IO::MergeInput{.path = result.path});
        rows += result.rows;
        rowGroups += result.rowGroups;
    }

    timer.Reset();
    const auto mergeStats = Columnar::IO::MergeIyxFiles(inputs, options.outputPath);

    std::cerr << "Merged: " << mergeStats.rows << " rows, "
              << mergeStats.rowGroups << " row groups, "
              << mergeStats.bytesCopied << " bytes copied in "
              << Columnar::Util::FormatSeconds(timer.ElapsedSeconds()) << '\n';

    if (mergeStats.rows != rows || mergeStats.rowGroups != rowGroups) {
        throw std::runtime_error("parallel conversion produced inconsistent merge stats");
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const Options options = ParseArgs(argc, argv);
        EnsureParentDirectory(options.outputPath);

        Columnar::Util::Timer timer;

        Columnar::Schema schema =
            Columnar::Parser::LoadSchemaFromCsv(options.schemaPath.string());
        std::cerr << "Schema: " << schema.GetColumnCount() << " columns\n";

        if (options.threads <= 1) {
            ConvertSequential(options, schema);
        } else {
            ConvertParallel(options, schema);
        }

        std::cerr << "Elapsed: "
                  << Columnar::Util::FormatSeconds(timer.ElapsedSeconds())
                  << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
