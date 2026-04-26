#include <io/csv/csv_reader.h>
#include <io/format/format_writer.h>
#include <parser/format/schema_parser.h>
#include <util/timer.h>

#include <exception>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: csv2iyx <schema.csv> <data.csv> <output.iyx>\n";
        return 1;
    }

    try {
        Columnar::Util::Timer timer;

        Columnar::Schema schema = Columnar::Parser::LoadSchemaFromCsv(argv[1]);
        std::cerr << "Schema: " << schema.GetColumnCount() << " columns\n";

        Columnar::IO::CsvReader reader(argv[2], schema);
        Columnar::IO::FormatWriter writer(argv[3]);

        writer.Begin(schema);

        size_t rowGroupNum = 0;
        while (auto rg = reader.ReadRowGroup()) {
            writer.WriteRowGroup(*rg);
            std::cerr << "Row group " << ++rowGroupNum << ": " << rg->GetRowCount()
                      << " rows\n";
        }

        writer.End();

        const double elapsed = timer.ElapsedSeconds();
        std::cerr << "Done! Total: " << reader.GetTotalRowsRead() << " rows, "
                  << writer.GetRowGroupCount() << " row groups\n";
        std::cerr << "Elapsed: " << Columnar::Util::FormatSeconds(elapsed)
                  << " ("
                  << Columnar::Util::FormatRowsPerSecond(
                         reader.GetTotalRowsRead(), elapsed)
                  << ")\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
