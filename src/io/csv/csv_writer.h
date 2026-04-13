#pragma once

#include <core/batch.h>

#include <fstream>
#include <string>

namespace Columnar::IO {

class CsvWriter {
public:
    explicit CsvWriter(const std::string& filename);

    void WriteBatch(const RowGroup& batch);
    void Flush();

    size_t GetRowsWritten() const;

    ~CsvWriter();

private:
    std::ofstream file_;
    size_t rowsWritten_ = 0;
};

}  // namespace Columnar::IO
