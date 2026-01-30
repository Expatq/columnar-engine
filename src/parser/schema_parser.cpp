#include <core/schema.h>
#include <core/types.h>
#include <parser/csv_parser.h>
#include <parser/schema_parser.h>

#include <util/str.h>

#include <fstream>
#include <stdexcept>
#include <string>

namespace Columnar::Parser {

ColumnSchema ParseSchemaLine(const std::string& line, size_t lineNumber) {
    auto fields = ParseCsvLine(line);

    if (fields.size() != 2) {
        throw std::runtime_error(
            "Invalid schema line " + std::to_string(lineNumber) +
            ": expected 'column_name,type_name', got: " + line);
    }

    std::string columnName = str::strip(fields[0]);
    std::string typeName = str::strip(fields[1]);

    if (columnName.empty()) {
        throw std::runtime_error("Invalid schema line " +
                                 std::to_string(lineNumber) +
                                 ": empty column name");
    }

    if (typeName.empty()) {
        throw std::runtime_error("Invalid schema line " +
                                 std::to_string(lineNumber) +
                                 ": empty type name");
    }

    Types::DataType type;
    try {
        type = Types::ParseDataType(typeName);
    } catch (const std::invalid_argument& inv_arg_exception) {
        throw std::runtime_error("Invalid schema line " +
                                 std::to_string(lineNumber) +
                                 ": unknown type '" + typeName + "'");
    }

    return ColumnSchema(columnName, type);
}

Schema LoadSchemaFromCsv(const std::string& filename) {
    std::ifstream input(filename);
    if (!input.is_open()) {
        throw std::runtime_error("Cannot open schema file: " + filename);
    }

    Schema schema;
    std::string line;
    size_t lineNumber = 0;

    while (std::getline(input, line)) {
        ++lineNumber;

        std::string stripped = str::strip(line);
        if (stripped.empty()) {
            continue;
        }

        ColumnSchema column = ParseSchemaLine(line, lineNumber);

        if (schema.HasColumn(column.name)) {
            throw std::runtime_error("Duplicate column name at line " +
                                     std::to_string(lineNumber) + ": " +
                                     column.name);
        }

        schema.AddColumn(column);
    }

    if (schema.IsEmpty()) {
        throw std::runtime_error("Schema file is empty: " + filename);
    }

    return schema;
}

void SaveSchemaToCsv(const Schema& schema, const std::string& outFilename) {
    std::ofstream output(outFilename);
    if (!output.is_open()) {
        throw std::runtime_error("Cannot create schema file: " + outFilename);
    }

    for (const auto& column : schema) {
        std::vector<std::string> fields = {column.name,
                                           Types::GetTypeName(column.type)};
        output << MergeFieldsInLine(fields) << '\n';
    }

    if (!output.good()) {
        throw std::runtime_error("Error writing schema file: " + outFilename);
    }
}

}  // namespace Columnar::Parser