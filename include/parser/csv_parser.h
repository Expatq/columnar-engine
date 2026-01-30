#pragma once

#include <string>
#include <vector>

namespace Columnar::Parser {

struct CsvParserOptions {
    char delimiter = ',';
    char quote = '"';
};

std::vector<std::string> ParseCsvLine(const std::string& line,
                                      const CsvParserOptions& options = {});

std::string EscapeCsvField(const std::string& field,
                           const CsvParserOptions& options = {});

bool IsFieldNeedEscpaing(const std::string& field,
                         const CsvParserOptions& options = {});

std::string MergeFieldsInLine(const std::vector<std::string>& fields,
                              const CsvParserOptions& options = {});

}  // namespace Columnar::Parser