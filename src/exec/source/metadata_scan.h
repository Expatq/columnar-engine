#pragma once

#include <exec/interface/operator.h>
#include <io/format/format_reader.h>
#include <optional>
#include "core/types.h"

namespace Columnar::Exec {

enum class MetadataField {
    TotalRowCount,
    RowGroupCount,
    ColCount,

    // TODO: support metadata fields below
    ColMin,
    ColMax,
    ColNullCount
};

struct MetadataColumn {
    MetadataField field;
    std::string outName;
    Types::LogicalType outType = Types::LogicalType::INT64;
};

class MetadataScan : public IOperator {
public:
    MetadataScan(std::string filepath, std::vector<MetadataColumn> columns);

    void Open() override;
    bool Next(ExecBatch& out) override;
    void Close() noexcept override;

private:
    int64_t ReadMetaField(MetadataField field) const; // TODO: support fields other than INT64

    std::string filepath_;
    std::vector<MetadataColumn> columns_;
    std::optional<IO::FormatReader> reader_;
    bool produced_;
};

}  // namespace Columnar::Exec
