#pragma once

#include <core/row_group.h>
#include <core/schema.h>
#include <core/types.h>

#include <fstream>
#include <optional>
#include <string>

namespace Columnar::IO {

class CsvReader {
public:
    CsvReader(const std::string& filename, const Schema& schema);

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

    std::optional<std::string> ReadLine();
    void AppendToBuffer(std::string&& line, RowGroup& batch);
    void ResetBuffers();
};

}  // namespace Columnar::IO
