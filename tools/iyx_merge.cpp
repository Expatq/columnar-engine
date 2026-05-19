#include <io/format/format_merger.h>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: iyx_merge <output.iyx> <part0.iyx> <part1.iyx> [...]\n";
        return EXIT_FAILURE;
    }

    try {
        std::vector<Columnar::IO::MergeInput> inputs;
        inputs.reserve(static_cast<size_t>(argc - 2));

        for (int i = 2; i < argc; ++i) {
            inputs.push_back(Columnar::IO::MergeInput{
                .path = std::filesystem::path(argv[i]),
            });
        }

        const auto stats =
            Columnar::IO::MergeIyxFiles(inputs, std::filesystem::path(argv[1]));

        std::cerr << "Merged " << stats.rowGroups << " row groups, "
                  << stats.rows << " rows, "
                  << stats.bytesCopied << " bytes copied\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
