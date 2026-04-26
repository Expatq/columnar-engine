#include "csv_writer.h"

#include <parser/csv/csv_parser.h>
#include <parser/format/serialize_to_string.h>

#include <stdexcept>
#include <vector>

namespace Columnar::IO {

CsvWriter::CsvWriter(const std::string& filename)
    : file_(filename) {
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot open CSV file for writing: " +
                                 filename);
    }
}

void CsvWriter::WriteRowGroup(const RowGroup& rg) {
    Schema schema = rg.GetSchema();

    for (size_t row = 0; row < rg.GetRowCount(); ++row) {
        std::vector<std::string> fields;
        fields.reserve(rg.GetColumnCount());

        for (size_t col = 0; col < rg.GetColumnCount(); ++col) {
            fields.push_back(Parser::FormatColumn(
                rg.GetColumn(col), row, schema.GetColumn(col).logical));
        }

        file_ << Parser::MergeFieldsInLine(fields) << '\n';
        ++rowsWritten_;
    }
}

void CsvWriter::Flush() {
    file_.flush();
}

size_t CsvWriter::GetRowsWritten() const {
    return rowsWritten_;
}

CsvWriter::~CsvWriter() {
    Flush();
    if (file_.is_open()) {
        file_.close();
    }
}

}  // namespace Columnar::IO
