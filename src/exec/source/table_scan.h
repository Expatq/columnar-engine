#pragma once

#include <exec/interface/operator.h>
#include <exec/core/required_columns.h>

#include <io/format/format_reader.h>

namespace Columnar::Exec {

class TableScan : public IOperator {
public:
    TableScan(std::string filepath, RequiredColumns requiredCols);

    void Open() override;
    bool Next(ExecBatch& out) override;
    void Close() noexcept override;

    const Schema& GetSchema() const;

private:
    std::string filepath_;
    RequiredColumns requiredCols_;
    std::optional<IO::FormatReader> reader_;

};

}  // namespace Columnar::Exec
