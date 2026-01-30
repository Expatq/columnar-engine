#pragma once

#include <core/schema.h>

#include <string>

namespace Columnar::Parser {

Schema LoadSchemaFromCsv(const std::string& filename);

void SaveSchemaToCsv(const Schema& schema, const std::string& outFilename);

ColumnSchema ParseSchemaLine(const std::string& line, size_t lineNumber = 0);

}  // namespace Columnar::Parser