#include "csv_range_reader.h"

#include <io/binary/file_ops.h>

#include <core/column.h>

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <charconv>
#include <cstring>
#include <stdexcept>
#include <system_error>

namespace Columnar::IO {

namespace {

constexpr size_t kReadBufferSize = 4_MB;
constexpr int64_t kSecondsPerDay = 86400;

void Trim(std::string_view* value) {
    while (!value->empty() && (value->front() == ' ' || value->front() == '\t')) {
        value->remove_prefix(1);
    }
    while (!value->empty() &&
           (value->back() == ' ' || value->back() == '\t' || value->back() == '\r')) {
        value->remove_suffix(1);
    }
}

template <typename T>
T ParseInt(std::string_view value, const char* typeName) {
    Trim(&value);
    if (value.empty()) {
        throw std::invalid_argument(std::string("cannot parse empty field as ") + typeName);
    }

    T result{};
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    auto [ptr, err] = std::from_chars(begin, end, result);
    if (err != std::errc{} || ptr != end) {
        throw std::invalid_argument(std::string("cannot parse field as ") + typeName);
    }
    return result;
}

int32_t DaysFromCivil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<int32_t>(era * 146097 + static_cast<int>(doe) - 719468);
}

int Digit(char ch) {
    if (ch < '0' || ch > '9') {
        throw std::invalid_argument("bad date/time digit");
    }
    return ch - '0';
}

int32_t ParseDate(std::string_view value) {
    Trim(&value);
    if (value.size() != 10 || value[4] != '-' || value[7] != '-') {
        throw std::invalid_argument("bad date format");
    }

    const int year =
        Digit(value[0]) * 1000 + Digit(value[1]) * 100 + Digit(value[2]) * 10 + Digit(value[3]);
    const unsigned month = static_cast<unsigned>(Digit(value[5]) * 10 + Digit(value[6]));
    const unsigned day = static_cast<unsigned>(Digit(value[8]) * 10 + Digit(value[9]));
    return DaysFromCivil(year, month, day);
}

int64_t ParseTimestamp(std::string_view value) {
    Trim(&value);
    if (value.size() != 19 || value[10] != ' ' || value[13] != ':' || value[16] != ':') {
        throw std::invalid_argument("bad timestamp format");
    }

    const int32_t days = ParseDate(value.substr(0, 10));
    const int hours = Digit(value[11]) * 10 + Digit(value[12]);
    const int minutes = Digit(value[14]) * 10 + Digit(value[15]);
    const int seconds = Digit(value[17]) * 10 + Digit(value[18]);

    return static_cast<int64_t>(days) * kSecondsPerDay +
           static_cast<int64_t>(hours) * 3600 +
           static_cast<int64_t>(minutes) * 60 +
           static_cast<int64_t>(seconds);
}

uint8_t ParseBool(std::string_view value) {
    Trim(&value);
    if (value == "true" || value == "1") {
        return 1;
    }
    if (value == "false" || value == "0") {
        return 0;
    }
    throw std::invalid_argument("bad bool field");
}

void AppendValue(Types::AnyColumnData& column,
                 Types::LogicalType logical,
                 std::string_view value) {
    switch (logical) {
        case Types::LogicalType::INT16:
            std::get<std::vector<int16_t>>(column).push_back(ParseInt<int16_t>(value, "int16"));
            return;
        case Types::LogicalType::INT32:
            std::get<std::vector<int32_t>>(column).push_back(ParseInt<int32_t>(value, "int32"));
            return;
        case Types::LogicalType::INT64:
            std::get<std::vector<int64_t>>(column).push_back(ParseInt<int64_t>(value, "int64"));
            return;
        case Types::LogicalType::INT128:
            std::get<std::vector<Int128>>(column).push_back(
                static_cast<Int128>(ParseInt<int64_t>(value, "int128")));
            return;
        case Types::LogicalType::BOOL:
            std::get<std::vector<uint8_t>>(column).push_back(ParseBool(value));
            return;
        case Types::LogicalType::STRING:
            std::get<std::vector<std::string>>(column).emplace_back(value);
            return;
        case Types::LogicalType::DATE:
            std::get<std::vector<int32_t>>(column).push_back(ParseDate(value));
            return;
        case Types::LogicalType::TIMESTAMP:
            std::get<std::vector<int64_t>>(column).push_back(ParseTimestamp(value));
            return;
    }
}

}  // namespace

CsvRangeReader::CsvRangeReader(std::filesystem::path path,
                               Schema schema,
                               CsvBoundaryIndexer::RecordRange range,
                               size_t rowGroupSize)
    : path_(std::move(path)),
      schema_(std::move(schema)),
      range_(range),
      rowGroupSize_(rowGroupSize),
      pos_(range.beginOffset) {
    if (rowGroupSize_ == 0) {
        throw std::invalid_argument("CsvRangeReader: row group size cannot be zero");
    }

    fd_ = ::open(path_.c_str(), O_RDONLY);
    if (fd_ < 0) {
        throw std::runtime_error("CsvRangeReader: cannot open " + path_.string() + ": " +
                                 std::strerror(errno));
    }

    readBuffer_.resize(kReadBufferSize);

    buffers_.reserve(schema_.GetColumnCount());
    for (const auto& col : schema_) {
        buffers_.push_back(Types::CreateEmptyColumnData(col.physical));
    }
}

CsvRangeReader::~CsvRangeReader() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

std::optional<RowGroup> CsvRangeReader::ReadRowGroup() {
    if (pos_ >= range_.endOffset && bufferPos_ >= bufferSize_) {
        return std::nullopt;
    }

    ResetBuffers();

    std::string record;
    size_t rowCount = 0;
    while (rowCount < rowGroupSize_ && ReadRecord(&record)) {
        if (record.empty()) {
            continue;
        }
        AppendRecord(record);
        ++rowCount;
        ++rowsRead_;
    }

    if (rowCount == 0) {
        return std::nullopt;
    }

    std::vector<Column> columns;
    columns.reserve(buffers_.size());
    for (size_t i = 0; i < buffers_.size(); ++i) {
        columns.emplace_back(std::move(buffers_[i]), schema_.GetColumn(i).physical);
    }

    return RowGroup(schema_, std::move(columns));
}

bool CsvRangeReader::Refill() {
    if (pos_ >= range_.endOffset) {
        bufferPos_ = 0;
        bufferSize_ = 0;
        return false;
    }

    const uint64_t remaining = range_.endOffset - pos_;
    bufferSize_ = static_cast<size_t>(std::min<uint64_t>(remaining, readBuffer_.size()));
    ReadAll(fd_, readBuffer_.data(), bufferSize_, pos_);
    bufferPos_ = 0;
    return bufferSize_ != 0;
}

bool CsvRangeReader::GetChar(char* ch) {
    if (bufferPos_ >= bufferSize_ && !Refill()) {
        return false;
    }

    *ch = readBuffer_[bufferPos_++];
    ++pos_;
    return true;
}

bool CsvRangeReader::PeekChar(char* ch) {
    if (bufferPos_ >= bufferSize_ && !Refill()) {
        return false;
    }

    *ch = readBuffer_[bufferPos_];
    return true;
}

bool CsvRangeReader::ReadRecord(std::string* out) {
    out->clear();
    if (pos_ >= range_.endOffset && bufferPos_ >= bufferSize_) {
        return false;
    }

    bool inQuotes = false;
    char ch = 0;
    while (GetChar(&ch)) {
        out->push_back(ch);

        if (ch == '"') {
            if (inQuotes) {
                char next = 0;
                if (PeekChar(&next) && next == '"') {
                    GetChar(&next);
                    out->push_back(next);
                    continue;
                }
            }
            inQuotes = !inQuotes;
            continue;
        }

        if (ch == '\n' && !inQuotes) {
            if (!out->empty() && out->back() == '\n') {
                out->pop_back();
            }
            if (!out->empty() && out->back() == '\r') {
                out->pop_back();
            }
            return true;
        }
    }

    if (inQuotes) {
        throw std::runtime_error("CsvRangeReader: unclosed quoted record");
    }
    return !out->empty();
}

void CsvRangeReader::AppendRecord(std::string_view record) {
    size_t colIdx = 0;
    size_t pos = 0;
    std::string scratch;

    while (true) {
        if (colIdx >= schema_.GetColumnCount()) {
            throw std::runtime_error("CsvRangeReader: too many fields at row " +
                                     std::to_string(range_.firstRow + rowsRead_));
        }

        scratch.clear();
        std::string_view field;

        if (pos < record.size() && record[pos] == '"') {
            ++pos;
            while (pos < record.size()) {
                const char ch = record[pos++];
                if (ch == '"') {
                    if (pos < record.size() && record[pos] == '"') {
                        scratch.push_back('"');
                        ++pos;
                        continue;
                    }
                    break;
                }
                scratch.push_back(ch);
            }
            field = scratch;
        } else {
            const size_t fieldBegin = pos;
            while (pos < record.size() && record[pos] != ',') {
                ++pos;
            }
            field = record.substr(fieldBegin, pos - fieldBegin);
        }

        AppendField(colIdx, field);
        ++colIdx;

        if (pos >= record.size()) {
            break;
        }
        if (record[pos] != ',') {
            throw std::runtime_error("CsvRangeReader: expected delimiter at row " +
                                     std::to_string(range_.firstRow + rowsRead_));
        }
        ++pos;
    }

    if (colIdx != schema_.GetColumnCount()) {
        throw std::runtime_error("CsvRangeReader: field count mismatch at row " +
                                 std::to_string(range_.firstRow + rowsRead_) +
                                 ": expected " +
                                 std::to_string(schema_.GetColumnCount()) +
                                 ", got " + std::to_string(colIdx));
    }
}

void CsvRangeReader::AppendField(size_t colIdx, std::string_view field) {
    AppendValue(buffers_[colIdx], schema_.GetColumn(colIdx).logical, field);
}

void CsvRangeReader::ResetBuffers() {
    for (auto& buffer : buffers_) {
        std::visit([&](auto& values) {
            values.clear();
            values.reserve(rowGroupSize_);
        },
                   buffer);
    }
}

}  // namespace Columnar::IO
