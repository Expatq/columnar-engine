#include "csv_reader.h"

#include <parser/csv/csv_parser.h>
#include <parser/format/value_parser.h>

#include <core/types.h>

#include <util/assert.h>

#include <stdexcept>

namespace Columnar::IO {

CsvReader::CsvReader(const std::string& filename, const Schema& schema)
    : file_(filename),
      schema_(schema) {
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot open CSV file: " + filename);
    }

    COLUMNAR_ASSERT(schema_.GetColumnCount() > 0,
                    "CsvReader: schema cannot be empty");

    buffers_.reserve(schema_.GetColumnCount());
    for (size_t i = 0; i < schema_.GetColumnCount(); ++i) {
        buffers_.push_back(
            Types::CreateEmptyColumnData(schema_.GetColumn(i).physical));
    }
}

std::optional<RowGroup> CsvReader::ReadRowGroup() {}

bool CsvReader::IsEnd() const {
    return !file_.good() || file_.eof();
}

size_t CsvReader::GetTotalRowsRead() const {
    return totalRowsRead_;
}

const Schema& CsvReader::GetSchema() const {
    return schema_;
}

std::optional<std::string> CsvReader::ReadLine() {}

void CsvReader::AppendToBuffer(size_t colIdx, const std::string& value) {
    const auto& field = schema_.GetColumn(colIdx);
    auto parsed = Parser::ParseValue(value, field.logical);
}

void CsvReader::ResetBuffers() {
    for (size_t i = 0; i < buffers_.size(); ++i) {
        buffers_[i] =
            Types::CreateEmptyColumnData(schema_.GetColumn(i).physical);
    }
}

}  // namespace Columnar::IO