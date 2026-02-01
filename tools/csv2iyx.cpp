#include <io/csv_reader.h>
#include <io/format_writer.h>
#include <parser/schema_parser.h>

#include <exception>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: csv2iyx <schema.csv> <data.csv> <output.iyx>\n";
        return 1;
    }

    try {
        Columnar::Schema schema = Columnar::Parser::LoadSchemaFromCsv(argv[1]);
        std::cerr << "Schema: " << schema.GetColumnCount() << " columns\n";

        Columnar::IO::CsvReader reader(argv[2], schema);
        Columnar::IO::FormatWriter writer(argv[3]);

        writer.Begin(schema);

        size_t batchNum = 0;
        while (auto batch = reader.ReadBatch()) {
            Columnar::RowGroup rg(std::move(*batch));
            writer.WriteRowGroup(rg);
            std::cerr << "Batch " << ++batchNum << ": "
                      << rg.GetBatch().GetRowCount() << " rows\n";
        }

        writer.End();

        std::cerr << "Done! Total: " << reader.GetTotalRowsRead() << " rows, "
                  << writer.GetRowGroupCount() << " row groups\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}