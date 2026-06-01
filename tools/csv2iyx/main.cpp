#include "converter.h"

#include <util/timer.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

[[noreturn]] void Usage(const char* program) {
    std::cerr << "Usage: " << program
              << " <schema.csv> <data.csv> <output.iyx>"
                 " [--threads N] [--row-group-size N]\n";
    std::exit(1);
}

std::string RequireValue(int argc, char** argv, int& i) {
    if (i + 1 >= argc)
        throw std::invalid_argument(std::string{argv[i]} + " requires a value");
    return argv[++i];
}

size_t ParsePositive(std::string_view value, std::string_view flag) {
    size_t result = 0;
    for (const char ch : value) {
        if (ch < '0' || ch > '9')
            throw std::invalid_argument(std::string{flag} + " must be a positive integer");
        result = result * 10 + static_cast<size_t>(ch - '0');
    }
    if (result == 0)
        throw std::invalid_argument(std::string{flag} + " must be > 0");
    return result;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 4) Usage(argv[0]);

    try {
        Columnar::ConvertOptions opts;
        opts.schemaPath = argv[1];
        opts.csvPath    = argv[2];
        opts.iyxPath    = argv[3];

        for (int i = 4; i < argc; ++i) {
            const std::string_view arg = argv[i];
            if (arg == "--threads") {
                opts.numThreads = ParsePositive(RequireValue(argc, argv, i), arg);
            } else if (arg == "--row-group-size") {
                opts.rowGroupSize = ParsePositive(RequireValue(argc, argv, i), arg);
            } else {
                throw std::invalid_argument("unknown argument: " + std::string{arg});
            }
        }

        const size_t effectiveThreads = opts.numThreads > 0
            ? opts.numThreads : 1;

        std::cerr << "Schema:         " << opts.schemaPath << '\n'
                  << "Threads:        " << effectiveThreads << '\n'
                  << "Row group size: " << opts.rowGroupSize << '\n';

        Columnar::Util::Timer timer;
        Columnar::Run(opts);

        std::cerr << "Elapsed: " << Columnar::Util::FormatSeconds(timer.ElapsedSeconds()) << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        Usage(argv[0]);
    }
}
