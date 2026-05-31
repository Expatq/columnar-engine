#include <exec/core/exec_batch.h>
#include <exec/core/operator_runner.h>
#include <exec/query/clickbench_queries.h>

#include <parser/format/serialize_to_string.h>

#include <core/row_group.h>
#include <core/schema.h>

#include <util/timer.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void AppendCsvField(std::string& line, const std::string& value) {
    if (value.find_first_of(",\"\n\r") == std::string::npos) {
        line += value;
        return;
    }
    line += '"';
    for (const char ch : value) {
        if (ch == '"') {
            line += '"';
        }
        line += ch;
    }
    line += '"';
}

void WriteRow(const Columnar::RowGroup& rg, size_t row) {
    const Columnar::Schema& schema = rg.GetSchema();
    const size_t cols = rg.GetColumnCount();

    std::string line;
    for (size_t c = 0; c < cols; ++c) {
        if (c > 0) {
            line += ',';
        }
        AppendCsvField(
            line, Columnar::Parser::FormatColumn(rg.GetColumn(c), row, schema.GetColumn(c).logical));
    }
    line += '\n';
    std::cout << line;
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

        auto root = Columnar::Exec::BuildQuery(path, queryId);
        Columnar::Exec::OperatorRunner runner(*root);

        Columnar::Util::Timer timer;
        runner.Open();

        size_t resultRows = 0;
        Columnar::Exec::ExecBatch batch;
        while (runner.Next(batch)) {
            if (!batch.rowGroup) {
                continue;
            }
            if (batch.has_selection) {
                for (const auto row : batch.selection.Rows()) {
                    WriteRow(*batch.rowGroup, row);
                    ++resultRows;
                }
            } else {
                for (size_t row = 0; row < batch.rowCount; ++row) {
                    WriteRow(*batch.rowGroup, row);
                    ++resultRows;
                }
            }
        }

        runner.Close();
        const int64_t elapsedMs = timer.ElapsedMilliseconds();
        std::cerr << "query=" << queryId << " elapsed_ms=" << elapsedMs
                  << " rows=" << resultRows << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
