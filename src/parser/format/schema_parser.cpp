#include "schema_parser.h"

#include <parser/csv/csv_parser.h>

#include <core/schema.h>
#include <core/types.h>

#include <util/str.h>

#include <fstream>
#include <stdexcept>
#include <string>
#include "util/assert.h"

namespace Columnar::Parser {

namespace {

ColumnSchema ParseSchemaLine(const std::string& line) {
    auto fields = ParseCsvLine(line);

    COLUMNAR_ASSERT(fields.size() == 2,
                    "invalid schema line, expected 'column_name,type_name', got: " +
                        line);

    std::string columnName = str::strip(fields[0]);
    std::string typeName = str::strip(fields[1]);

    COLUMNAR_ASSERT(!columnName.empty(), "schema contains empty column name");
    COLUMNAR_ASSERT(!typeName.empty(), "schema contains empty type name");

    Types::LogicalType type = Types::ParseLogicalType(typeName);

    return ColumnSchema(columnName, type);
}

}  // namespace

Schema LoadSchemaFromCsv(const std::string& filename) {
    std::ifstream input(filename);
    COLUMNAR_ASSERT(input.is_open(),
                    "cannot open schema file: " + filename);

    Schema schema;
    std::string line;

    while (std::getline(input, line)) {
        std::string stripped = str::strip(line);
        if (stripped.empty()) {
            continue;
        }

        ColumnSchema column = ParseSchemaLine(line);
        COLUMNAR_ASSERT(!schema.FindColumn(column.name),
                        "duplicate column name in schema '" +
                            column.name + "'");

        schema.AddColumn(column);
    }
    COLUMNAR_ASSERT(
        !schema.IsEmpty(),
        "schema file is empty '" + filename + "'");

    input.close();
    return schema;
}

void SaveSchemaToCsv(const Schema& schema, const std::string& outFilename) {
    std::ofstream output(outFilename);
    COLUMNAR_ASSERT(output.is_open(),
                    "Cannot create schema file: " + outFilename);

    for (const auto& column : schema) {
        std::vector<std::string> fields = {
            column.name, Types::GetLogicalTypeName(column.logical)};
        output << MergeFieldsInLine(fields) << '\n';
    }

    COLUMNAR_ASSERT(output.good(), "Error writing schema file: " + outFilename);
    output.close();
}

}  // namespace Columnar::Parser
