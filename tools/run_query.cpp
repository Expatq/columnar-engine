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

std::unique_ptr<Columnar::Exec::IOperator> BuildQuery(
    const std::string& path, int queryId) {
    switch (queryId) {
        case 1:
            return Columnar::Exec::BuildQ1(path);
        case 2:
            return Columnar::Exec::BuildQ2(path);
        case 3:
            return Columnar::Exec::BuildQ3(path);
        case 4:
            return Columnar::Exec::BuildQ4(path);
        case 5:
            return Columnar::Exec::BuildQ5(path);
        case 6:
            return Columnar::Exec::BuildQ6(path);
        case 7:
            return Columnar::Exec::BuildQ7(path);
        case 8:
            return Columnar::Exec::BuildQ8(path);
        case 9:
            return Columnar::Exec::BuildQ9(path);
        case 10:
            return Columnar::Exec::BuildQ10(path);
        case 11:
            return Columnar::Exec::BuildQ11(path);
        case 12:
            return Columnar::Exec::BuildQ12(path);
        case 13:
            return Columnar::Exec::BuildQ13(path);
        case 14:
            return Columnar::Exec::BuildQ14(path);
        case 15:
            return Columnar::Exec::BuildQ15(path);
        case 16:
            return Columnar::Exec::BuildQ16(path);
        case 17:
            return Columnar::Exec::BuildQ17(path);
        case 18:
            return Columnar::Exec::BuildQ18(path);
        case 19:
            return Columnar::Exec::BuildQ19(path);
        case 20:
            return Columnar::Exec::BuildQ20(path);
        default:
            throw std::invalid_argument("unsupported query id: " +
                                        std::to_string(queryId));
    }
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
        std::cerr << "usage: run_query <file.iyx> <query_id>\n";
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
