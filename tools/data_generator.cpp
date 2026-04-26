#include <core/types.h>

#include <cctype>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using Columnar::Types::LogicalType;

struct Options {
    uint64_t rows = 0;
    size_t columns = 0;
    uint64_t targetBytes = 0;
    std::string dataFile;
    std::string schemaFile;
};

std::string ToLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

uint64_t ParseUnsigned(std::string_view value, std::string_view name) {
    if (value.empty()) {
        throw std::invalid_argument(std::string{name} + " cannot be empty");
    }

    uint64_t result = 0;
    for (char ch : value) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            throw std::invalid_argument(std::string{name} +
                                        " must be an unsigned integer");
        }
        const uint64_t digit = static_cast<uint64_t>(ch - '0');
        if (result >
            (std::numeric_limits<uint64_t>::max() - digit) / 10) {
            throw std::overflow_error(std::string{name} + " is too large");
        }
        result = result * 10 + digit;
    }

    return result;
}

uint64_t ParseSize(std::string_view raw) {
    if (raw.empty()) {
        throw std::invalid_argument("size cannot be empty");
    }

    size_t numberEnd = 0;
    while (numberEnd < raw.size() &&
           std::isdigit(static_cast<unsigned char>(raw[numberEnd]))) {
        ++numberEnd;
    }

    const uint64_t value = ParseUnsigned(raw.substr(0, numberEnd), "size");
    const std::string suffix = ToLower(std::string(raw.substr(numberEnd)));

    uint64_t multiplier = 1;
    if (suffix.empty() || suffix == "b") {
        multiplier = 1;
    } else if (suffix == "k" || suffix == "kb" || suffix == "kib") {
        multiplier = 1024ull;
    } else if (suffix == "m" || suffix == "mb" || suffix == "mib") {
        multiplier = 1024ull * 1024ull;
    } else if (suffix == "g" || suffix == "gb" || suffix == "gib") {
        multiplier = 1024ull * 1024ull * 1024ull;
    } else {
        throw std::invalid_argument("unsupported size suffix: " + suffix);
    }

    if (value > std::numeric_limits<uint64_t>::max() / multiplier) {
        throw std::overflow_error("size is too large");
    }
    return value * multiplier;
}

void PrintUsage(const char* program) {
    std::cerr
        << "Usage: " << program
        << " --rows N --columns N --size SIZE --data data.csv --schema schema.csv\n"
        << "\n"
        << "SIZE suffixes: B, KB, MB, GB (binary 1024-based units).\n"
        << "Example: " << program
        << " --rows 100000 --columns 16 --size 64MB --data data.csv --schema schema.csv\n";
}

Options ParseArgs(int argc, char* argv[]) {
    Options options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto requireValue = [&](std::string_view flag) -> std::string {
            if (i + 1 >= argc) {
                throw std::invalid_argument(std::string{flag} +
                                            " requires a value");
            }
            return argv[++i];
        };

        if (arg == "--rows") {
            options.rows = ParseUnsigned(requireValue(arg), "rows");
        } else if (arg == "--columns") {
            const uint64_t columns = ParseUnsigned(requireValue(arg), "columns");
            if (columns > std::numeric_limits<size_t>::max()) {
                throw std::overflow_error("columns is too large");
            }
            options.columns = static_cast<size_t>(columns);
        } else if (arg == "--size") {
            options.targetBytes = ParseSize(requireValue(arg));
        } else if (arg == "--data") {
            options.dataFile = requireValue(arg);
        } else if (arg == "--schema") {
            options.schemaFile = requireValue(arg);
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            std::exit(0);
        } else {
            throw std::invalid_argument("unknown argument: " + arg);
        }
    }

    if (options.rows == 0) {
        throw std::invalid_argument("--rows must be positive");
    }
    if (options.columns == 0) {
        throw std::invalid_argument("--columns must be positive");
    }
    if (options.targetBytes == 0) {
        throw std::invalid_argument("--size must be positive");
    }
    if (options.dataFile.empty()) {
        throw std::invalid_argument("--data is required");
    }
    if (options.schemaFile.empty()) {
        throw std::invalid_argument("--schema is required");
    }

    return options;
}

std::vector<LogicalType> BuildSchema(size_t columns) {
    std::vector<LogicalType> kinds;
    kinds.reserve(columns);

    for (size_t i = 0; i < columns; ++i) {
        if (i + 1 == columns) {
            kinds.push_back(LogicalType::STRING);
            continue;
        }

        switch (i % 4) {
            case 0:
                kinds.push_back(LogicalType::INT64);
                break;
            case 1:
                kinds.push_back(LogicalType::INT32);
                break;
            case 2:
                kinds.push_back(LogicalType::BOOL);
                break;
            default:
                kinds.push_back(LogicalType::STRING);
                break;
        }
    }

    return kinds;
}

uint64_t Int64Value(uint64_t row, size_t column) {
    return row * 1'000'003ull + static_cast<uint64_t>(column + 17);
}

uint32_t Int32Value(uint64_t row, size_t column) {
    return static_cast<uint32_t>((row * 97ull + column * 13ull) %
                                 1'000'000ull);
}

bool BoolValue(uint64_t row, size_t column) {
    return ((row + column) % 2) == 0;
}

std::string FieldWithoutStringPayload(LogicalType kind,
                                      uint64_t row,
                                      size_t column) {
    switch (kind) {
        case LogicalType::INT64:
            return std::to_string(Int64Value(row, column));
        case LogicalType::INT32:
            return std::to_string(Int32Value(row, column));
        case LogicalType::BOOL:
            return BoolValue(row, column) ? "true" : "false";
        case LogicalType::STRING:
            return {};
        default:
            throw std::logic_error("unsupported generated logical type");
    }
}

uint64_t MinLineSize(uint64_t row, const std::vector<LogicalType>& schema) {
    uint64_t size = 1;  // newline
    if (!schema.empty()) {
        size += schema.size() - 1;  // delimiters
    }

    for (size_t col = 0; col < schema.size(); ++col) {
        size += FieldWithoutStringPayload(schema[col], row, col).size();
    }

    return size;
}

uint64_t CountStringColumns(const std::vector<LogicalType>& schema) {
    uint64_t count = 0;
    for (LogicalType kind : schema) {
        if (kind == LogicalType::STRING) {
            ++count;
        }
    }
    return count;
}

uint64_t ComputeMinDataBytes(uint64_t rows,
                             const std::vector<LogicalType>& schema) {
    uint64_t total = 0;
    for (uint64_t row = 0; row < rows; ++row) {
        const uint64_t lineSize = MinLineSize(row, schema);
        if (total > std::numeric_limits<uint64_t>::max() - lineSize) {
            throw std::overflow_error("minimum generated data size is too large");
        }
        total += lineSize;
    }
    return total;
}

void WriteSchemaFile(const std::string& filename,
                     const std::vector<LogicalType>& schema) {
    std::ofstream output(filename);
    if (!output.is_open()) {
        throw std::runtime_error("cannot create schema file: " + filename);
    }

    for (size_t i = 0; i < schema.size(); ++i) {
        output << "col" << i << ','
               << Columnar::Types::GetLogicalTypeName(schema[i]) << '\n';
    }
}

void WriteStringPayload(std::ofstream& output,
                        uint64_t row,
                        size_t column,
                        uint64_t length) {
    static constexpr size_t kChunkSize = 4096;
    char buffer[kChunkSize];

    uint64_t written = 0;
    while (written < length) {
        const size_t chunk = static_cast<size_t>(
            std::min<uint64_t>(kChunkSize, length - written));
        for (size_t i = 0; i < chunk; ++i) {
            const uint64_t value =
                row * 131ull + static_cast<uint64_t>(column) * 17ull +
                written + i;
            buffer[i] = static_cast<char>('a' + (value % 26));
        }
        output.write(buffer, static_cast<std::streamsize>(chunk));
        written += chunk;
    }
}

void WriteDataFile(const std::string& filename,
                   uint64_t rows,
                   const std::vector<LogicalType>& schema,
                   uint64_t targetBytes,
                   uint64_t minBytes) {
    std::ofstream output(filename);
    if (!output.is_open()) {
        throw std::runtime_error("cannot create data file: " + filename);
    }

    const uint64_t stringColumns = CountStringColumns(schema);
    uint64_t extraBytes = targetBytes > minBytes ? targetBytes - minBytes : 0;
    uint64_t remainingStringCells = rows * stringColumns;

    for (uint64_t row = 0; row < rows; ++row) {
        for (size_t col = 0; col < schema.size(); ++col) {
            if (col > 0) {
                output << ',';
            }

            const LogicalType kind = schema[col];
            if (kind == LogicalType::STRING) {
                uint64_t payloadLength = 0;
                if (remainingStringCells > 0) {
                    payloadLength = extraBytes / remainingStringCells;
                    if (extraBytes % remainingStringCells != 0) {
                        ++payloadLength;
                    }
                    extraBytes -= payloadLength;
                    --remainingStringCells;
                }
                WriteStringPayload(output, row, col, payloadLength);
            } else {
                output << FieldWithoutStringPayload(kind, row, col);
            }
        }
        output << '\n';
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const Options options = ParseArgs(argc, argv);
        const std::vector<LogicalType> schema = BuildSchema(options.columns);

        const uint64_t minBytes = ComputeMinDataBytes(options.rows, schema);
        if (options.targetBytes < minBytes) {
            std::cerr << "Warning: requested size " << options.targetBytes
                      << " bytes is smaller than minimum possible " << minBytes
                      << " bytes for given rows/columns. Generating minimum size.\n";
        }

        WriteSchemaFile(options.schemaFile, schema);
        WriteDataFile(options.dataFile, options.rows, schema, options.targetBytes,
                      minBytes);

        const uint64_t actualBytes =
            std::filesystem::file_size(options.dataFile);
        std::cerr << "Generated schema: " << options.schemaFile << " ("
                  << options.columns << " columns)\n";
        std::cerr << "Generated data: " << options.dataFile << " ("
                  << options.rows << " rows, " << actualBytes << " bytes)\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        PrintUsage(argv[0]);
        return 1;
    }

    return 0;
}
