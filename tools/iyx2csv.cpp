#include <io/csv_writer.h>
#include <io/format_reader.h>
#include <parser/schema_parser.h>

#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: iyx2csv <input.iyx> <data.csv> <schema.csv>\n";
        return 1;
    }

    try {
        Columnar::IO::FormatReader reader(argv[1]);
        reader.Open();

        std::cerr << "Schema: " << reader.GetSchema().GetColumnCount()
                  << " columns\n";
        std::cerr << "Total: " << reader.GetTotalRowCount() << " rows, "
                  << reader.GetRowGroupCount() << " row groups\n";

        Columnar::Parser::SaveSchemaToCsv(reader.GetSchema(), argv[3]);
        std::cerr << "Schema saved to: " << argv[3] << "\n";

        Columnar::IO::CsvWriter writer(argv[2]);

        for (size_t i = 0; i < reader.GetRowGroupCount(); ++i) {
            Columnar::RowGroup rg = reader.ReadRowGroup(i);
            writer.WriteBatch(rg.GetBatch());
            std::cerr << "RowGroup " << i << ": " << rg.GetBatch().GetRowCount()
                      << " rows\n";
        }

        writer.Flush();
        std::cerr << "Done! Written: " << writer.GetRowsWritten() << " rows\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}