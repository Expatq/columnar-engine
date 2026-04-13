#include "csv_reader.h"

#include <parser/csv/csv_parser.h>
#include <parser/format/value_parser.h>

#include <util/assert.h>

#include <stdexcept>
#include "core/types.h"

namespace Columnar::IO {

CsvReader::CsvReader(const std::string& filename, const Schema& schema)
    : file_(filename),
      schema_(schema) {
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot open CSV file: " + filename);
    }

    COLUMNAR_ASSERT(schema_.GetColumnCount() > 0, "CsvReader: schema cannot be empty");

    buffers_.resize(schema_.GetColumnCount());
    ResetBuffers();
}



void CsvReader::ResetBuffers() {
    for (size_t i = 0; i < buffers_.size(); ++i) {
        buffers_[i] = Types::CreateEmptyColumnData(schema_.GetColumn(i).type);
    }
}

}  // namespace Columnar::IO