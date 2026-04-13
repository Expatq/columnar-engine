#include <parser/csv_parser.h>

#include <stdexcept>

namespace Columnar::Parser {

std::vector<std::string> ParseCsvLine(const std::string& line,
                                      const CsvParserOptions& options) {
    std::vector<std::string> fields;
    std::string currentField;
    bool inQuotes = false;

    size_t i = 0;
    while (i < line.size()) {
        char c = line[i];

        if (inQuotes) {
            if (c == options.quote) {
                // escape quote ?
                if (i + 1 < line.size() && line[i + 1] == options.quote) {
                    currentField += options.quote;
                    i += 2;
                } else {
                    inQuotes = false;
                    ++i;
                }
            } else {
                currentField += c;
                ++i;
            }
        } else {
            if (c == options.quote) {
                inQuotes = true;
                ++i;
            } else if (c == options.delimiter) {
                fields.push_back(currentField);
                currentField.clear();
                ++i;
            } else if (c == '\r') {  // windows compatible bullshit
                ++i;
            } else {
                currentField += c;
                ++i;
            }
        }
    }

    fields.push_back(currentField);

    if (inQuotes) {
        throw std::runtime_error("Unclosed quote in CSV line");
    }

    return fields;
}

bool IsFieldNeedEscpaing(const std::string& field,
                         const CsvParserOptions& options) {
    for (char c : field) {
        if (c == options.delimiter || c == options.quote || c == '\n' ||
            c == '\r') {
            return true;
        }
    }

    return false;
}

std::string EscapeCsvField(const std::string& field,
                           const CsvParserOptions& options) {
    constexpr static size_t kReserveAdditional = 10;

    if (!IsFieldNeedEscpaing(field, options)) {
        return field;
    }

    std::string result;
    result.reserve(field.size() + kReserveAdditional);
    result += options.quote;

    for (char c : field) {
        if (c == options.quote) {
            result += options.quote;
        }
        result += c;
    }

    result += options.quote;
    return result;
}

std::string MergeFieldsInLine(const std::vector<std::string>& fields,
                              const CsvParserOptions& options) {
    std::string result;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            result += options.delimiter;
        }
        result += EscapeCsvField(fields[i], options);
    }

    return result;
}

}  // namespace Columnar::Parser