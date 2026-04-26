#pragma once

#include <core/row_group.h>

#include <fstream>
#include <string>

namespace Columnar::IO {

class CsvWriter {
public:
    explicit CsvWriter(const std::string& filename);

    void WriteRowGroup(const RowGroup& rg);
    void Flush();

    size_t GetRowsWritten() const;

    ~CsvWriter();

private:
    std::ofstream file_;
    size_t rowsWritten_ = 0;
};

}  // namespace Columnar::IO
