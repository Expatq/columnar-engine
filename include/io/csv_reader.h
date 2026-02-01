#pragma once

#include <core/batch.h>
#include <core/schema.h>

#include <fstream>
#include <optional>
#include <string>

namespace Columnar::IO {

class CsvReader {
public:
    CsvReader(const std::string& filename, const Schema& schema);

    std::optional<Batch> ReadBatch();
    bool IsEnd() const;

    const Schema& GetSchema() const;
    size_t GetTotalRowsRead() const;

    ~CsvReader();

private:
    std::ifstream file_;
    Schema schema_;
    size_t totalRowsRead_ = 0;
    size_t lineNumber_ = 0;

    std::optional<std::string> ReadLine();
    void ParseAndAppendRow(std::string&& line, Batch& batch);
};

}  // namespace Columnar::IO
