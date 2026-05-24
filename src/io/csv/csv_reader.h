#pragma once

#include <core/row_group.h>
#include <core/schema.h>
#include <core/types.h>
#include <io/format/format_defs.h>

#include <fstream>
#include <optional>
#include <string>

namespace Columnar::IO {

class CsvReader {
public:
    CsvReader(const std::string& filename, const Schema& schema, size_t rowGroupSize = kDefaultRowGroupSize);

    std::optional<RowGroup> ReadRowGroup();

    bool IsEnd() const;
    size_t GetTotalRowsRead() const;
    const Schema& GetSchema() const;

    ~CsvReader();

private:
    std::ifstream file_;
    Schema schema_;
    std::vector<Types::AnyColumnData> buffers_;
    size_t totalRowsRead_ = 0;
    size_t lineNumber_ = 0;
    size_t rowGroupSize_;

    std::optional<std::string> ReadLine();
    void AppendToBuffer(size_t colIdx, const std::string& value);
    void ResetBuffers();
};

}  // namespace Columnar::IO
