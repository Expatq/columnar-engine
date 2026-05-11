#include "csv_reader.h"

#include <parser/csv/csv_parser.h>
#include <parser/format/value_parser.h>

#include <core/column.h>
#include <core/row_group.h>
#include <core/types.h>

#include <util/assert.h>
#include <util/int128.h>

#include <optional>
#include <stdexcept>
#include <string>

namespace Columnar::IO {

CsvReader::CsvReader(const std::string& filename, const Schema& schema)
    : file_(filename),
      schema_(schema) {
    if (!file_.is_open()) {
        throw std::runtime_error("Cannot open CSV file: " + filename);
    }
    COLUMNAR_ASSERT(schema_.GetColumnCount() > 0,
                    "schema cannot be empty");

    buffers_.reserve(schema_.GetColumnCount());
    for (size_t i = 0; i < schema_.GetColumnCount(); ++i) {
        buffers_.push_back(
            Types::CreateEmptyColumnData(schema_.GetColumn(i).physical));
    }
}

std::optional<RowGroup> CsvReader::ReadRowGroup() {
    if (IsEnd()) {
        return std::nullopt;
    }

    ResetBuffers();
    size_t rowCount = 0;

    while (rowCount < kBatchSize && !IsEnd()) {
        auto line = ReadLine();
        if (!line || line->empty()) {
            continue;
        }

        auto fields = Parser::ParseCsvLine(*line);

        if (fields.size() != schema_.GetColumnCount()) {
            throw std::runtime_error(
                "CsvReader: field count mismatch at line " +
                std::to_string(lineNumber_) + ": expected " +
                std::to_string(schema_.GetColumnCount()) + ", got " +
                std::to_string(fields.size()));
        }

        for (size_t i = 0; i < fields.size(); ++i) {
            AppendToBuffer(i, fields[i]);
        }

        ++rowCount;
        ++totalRowsRead_;
    }

    if (rowCount == 0) {
        return std::nullopt;
    }

    std::vector<Column> cols;
    cols.reserve(buffers_.size());
    for (size_t i = 0; i < buffers_.size(); ++i) {
        cols.emplace_back(std::move(buffers_[i]), schema_.GetColumn(i).physical);
    }

    return RowGroup(schema_, std::move(cols));
}

bool CsvReader::IsEnd() const {
    return !file_.good() || file_.eof();
}

size_t CsvReader::GetTotalRowsRead() const {
    return totalRowsRead_;
}

const Schema& CsvReader::GetSchema() const {
    return schema_;
}

std::optional<std::string> CsvReader::ReadLine() {
    std::string line;
    if (std::getline(file_, line)) {
        ++lineNumber_;
        return line;
    }
    return std::nullopt;
}

void CsvReader::AppendToBuffer(size_t colIdx, const std::string& value) {
    const auto& field = schema_.GetColumn(colIdx);
    auto parsed = Parser::ParseValue(value, field.logical);

    std::visit(
        Types::overloaded{
            [&](std::vector<int16_t>& v) {
                v.push_back(std::get<int16_t>(parsed));
            },
            [&](std::vector<int32_t>& v) {
                v.push_back(std::get<int32_t>(parsed));
            },
            [&](std::vector<int64_t>& v) {
                v.push_back(std::get<int64_t>(parsed));
            },
            [&](std::vector<uint8_t>& v) {
                v.push_back(std::get<uint8_t>(parsed));
            },
            [&](std::vector<std::string>& v) {
                v.push_back(std::get<std::string>(parsed));
            },
            [&](std::vector<Int128>& v) {
                v.push_back(std::get<Int128>(parsed));
            },
        },
        buffers_[colIdx]);
}

void CsvReader::ResetBuffers() {
    for (size_t i = 0; i < buffers_.size(); ++i) {
        buffers_[i] =
            Types::CreateEmptyColumnData(schema_.GetColumn(i).physical);
    }
}

CsvReader::~CsvReader() {
    if (file_.is_open()) {
        file_.close();
    }
}

}  // namespace Columnar::IO
