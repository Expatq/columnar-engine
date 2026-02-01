#include <core/batch.h>
#include <core/schema.h>

#include <io/csv_reader.h>
#include <parser/csv_parser.h>

#include <optional>
#include <stdexcept>

namespace Columnar::IO {

CsvReader::CsvReader(const std::string& filename, const Schema& schema)
    : file_(filename),
      schema_(schema) {

    if (!file_.is_open()) {
        throw std::runtime_error("Cannot open CSV file: " + filename);
    }

    if (schema.IsEmpty()) {
        throw std::invalid_argument("Schema cannot be empty");
    }
}

std::optional<Batch> CsvReader::ReadBatch() {
    if (IsEnd()) {
        return std::nullopt;
    }

    Batch batch = Batch::CreateEmpty(schema_);
    batch.Reserve(kBatchSize);

    while (!batch.IsFull() && !IsEnd()) {
        auto line = ReadLine();
        if (!line || line->empty()) {
            continue;
        }
        ParseAndAppendRow(std::move(*line), batch);
    }

    if (batch.IsEmpty()) {
        return std::nullopt;
    }

    return batch;
}

bool CsvReader::IsEnd() const {
    return (!file_.good()) || (file_.eof());
}

std::optional<std::string> CsvReader::ReadLine() {
    std::string line;
    if (std::getline(file_, line)) {
        ++lineNumber_;
        return line;
    }

    return std::nullopt;
}

void CsvReader::ParseAndAppendRow(std::string&& line, Batch& batch) {
    auto fields = Parser::ParseCsvLine(std::move(line));

    if (fields.size() != schema_.GetColumnCount()) {
        throw std::runtime_error("Field count mismatch at line " +
                                 std::to_string(lineNumber_) + ": expected " +
                                 std::to_string(schema_.GetColumnCount()) +
                                 ", got " + std::to_string(fields.size()));
    }

    batch.AppendRow(std::move(fields));
    ++totalRowsRead_;
}

const Schema& CsvReader::GetSchema() const {
    return schema_;
}

size_t CsvReader::GetTotalRowsRead() const {
    return totalRowsRead_;
}

CsvReader::~CsvReader() {
    if (file_.is_open()) {
        file_.close();
    }
}

}  // namespace Columnar::IO