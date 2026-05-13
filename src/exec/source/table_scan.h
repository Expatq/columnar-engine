#pragma once

#include <exec/core/required_columns.h>
#include <exec/interface/operator.h>

#include <io/format/format_reader.h>

#include <cstdint>
#include <vector>

namespace Columnar::Exec {

class TableScan : public IOperator {
public:
    TableScan(std::string filepath, RequiredColumns requiredCols);

    void Open() override;
    bool Next(ExecBatch& out) override;
    void Close() noexcept override;

    const Schema& GetSchema() const;

    void AddRangePredicate(size_t colIdx, int64_t lo, int64_t hi);
    void AddEqualityPredicate(size_t colIdx, int64_t value);

private:
    bool ShouldSkip(size_t rgIdx) const;

    struct RangePred {
        size_t colIdx;
        int64_t lo;
        int64_t hi;
    };

    std::string filepath_;
    RequiredColumns requiredCols_;
    std::optional<IO::FormatReader> reader_;
    std::vector<RangePred> rangePredicates_;
};

}  // namespace Columnar::Exec
