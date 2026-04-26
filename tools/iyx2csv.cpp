#include <io/csv/csv_writer.h>
#include <io/format/format_reader.h>
#include <parser/format/schema_parser.h>
#include <util/timer.h>

#include <exception>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: iyx2csv <input.iyx> <data.csv> <schema.csv>\n";
        return 1;
    }

    try {
        Columnar::Util::Timer timer;

        Columnar::IO::FormatReader reader(argv[1]);

        std::cerr << "Schema: " << reader.GetSchema().GetColumnCount()
                  << " columns\n";
        std::cerr << "Total: " << reader.GetTotalRowCount() << " rows, "
                  << reader.GetRowGroupCount() << " row groups\n";

        Columnar::Parser::SaveSchemaToCsv(reader.GetSchema(), argv[3]);
        std::cerr << "Schema saved to: " << argv[3] << "\n";

        Columnar::IO::CsvWriter writer(argv[2]);

        size_t i = 0;
        while (auto rg = reader.ReadRowGroup()) {
            writer.WriteRowGroup(*rg);
            std::cerr << "Row group " << i << ": " << rg->GetRowCount()
                      << " rows\n";
            ++i;
        }

        writer.Flush();
        const double elapsed = timer.ElapsedSeconds();
        std::cerr << "Done! Written: " << writer.GetRowsWritten() << " rows\n";
        std::cerr << "Elapsed: " << Columnar::Util::FormatSeconds(elapsed)
                  << " ("
                  << Columnar::Util::FormatRowsPerSecond(
                         writer.GetRowsWritten(), elapsed)
                  << ")\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
