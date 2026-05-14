#include <core/row_group.h>
#include <exec/core/exec_batch.h>
#include <exec/core/operator_runner.h>
#include <exec/query/clickbench_queries.h>
#include <parser/format/serialize_to_string.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

using Builder = std::unique_ptr<Columnar::Exec::IOperator> (*)(const std::string&);

constexpr Builder kBuilders[] = {
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

std::unique_ptr<Columnar::Exec::IOperator> BuildQuery(
    const std::string& path, int queryId) {
    if (queryId < 0 || queryId >= static_cast<int>(std::size(kBuilders))) {
        throw std::invalid_argument("unsupported query id: " +
                                    std::to_string(queryId));
    }
    return kBuilders[queryId](path);
}

void PrintRowGroup(const Columnar::RowGroup& rg) {
    for (size_t row = 0; row < rg.GetRowCount(); ++row) {
        for (size_t col = 0; col < rg.GetColumnCount(); ++col) {
            if (col != 0) {
                std::cout << '\t';
            }
            std::visit(
                [&](const auto& values) {
                    using T = typename std::decay_t<decltype(values)>::value_type;
                    if constexpr (std::is_same_v<T, std::string>) {
                        std::cout << values[row];
                    } else {
                        std::cout << static_cast<long long>(values[row]);
                    }
                },
                rg.GetColumn(col).GetData());
        }
        std::cout << '\n';
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: run_query <file.iyx> <query_id 0..42>\n";
        return EXIT_FAILURE;
    }

    try {
        const std::string path = argv[1];
        const int queryId = std::stoi(argv[2]);

        auto root = BuildQuery(path, queryId);
        Columnar::Exec::OperatorRunner runner(*root);
        runner.Open();

        Columnar::Exec::ExecBatch batch;
        while (runner.Next(batch)) {
            if (batch.rowGroup) {
                PrintRowGroup(*batch.rowGroup);
            }
        }

        runner.Close();
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
