#pragma once

#include "csv_boundary_index.h"

#include <core/row_group.h>
#include <core/schema.h>
#include <core/types.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Columnar::IO {

class CsvRangeReader {
public:
    CsvRangeReader(std::filesystem::path path,
                   Schema schema,
                   CsvBoundaryIndexer::RecordRange range,
                   size_t rowGroupSize);
    ~CsvRangeReader();

    CsvRangeReader(const CsvRangeReader&) = delete;
    CsvRangeReader& operator=(const CsvRangeReader&) = delete;

    std::optional<RowGroup> ReadRowGroup();

    uint64_t GetRowsRead() const {
        return rowsRead_;
    }

private:
    bool Refill();
    bool GetChar(char* ch);
    bool PeekChar(char* ch);

    bool ReadRecord(std::string* out);
    void AppendRecord(std::string_view record);
    void AppendField(size_t colIdx, std::string_view field);
    void ResetBuffers();

private:
    int fd_ = -1;

    std::filesystem::path path_;
    Schema schema_;
    CsvBoundaryIndexer::RecordRange range_;

    size_t rowGroupSize_ = 0;
    uint64_t pos_ = 0;
    uint64_t rowsRead_ = 0;

    std::vector<char> readBuffer_;
    size_t bufferPos_ = 0;
    size_t bufferSize_ = 0;

    std::vector<Types::AnyColumnData> buffers_;
};

}  // namespace Columnar::IO
