#include <core/batch.h>

#include <io/csv_writer.h>
#include <parser/csv_parser.h>

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

void CsvWriter::WriteBatch(const Batch& batch) {
    Schema schema = batch.GetSchema();

    for (size_t row = 0; row < batch.GetRowCount(); ++row) {
        std::vector<std::string> fields;
        fields.reserve(batch.GetColumnCount());

        for (size_t col = 0; col < batch.GetColumnCount(); ++col) {
            fields.push_back(batch.GetColumn(col).GetValueAsString(row));
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
