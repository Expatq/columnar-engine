#include "metadata_scan.h"

#include <exec/result_format/row_group_builder.h>

#include <stdexcept>

namespace Columnar::Exec {

MetadataScan::MetadataScan(std::string filepath, std::vector<MetadataColumn> columns)
    : filepath_(std::move(filepath)),
      columns_(std::move(columns)),
      produced_(false) {
}

void MetadataScan::Open() {
    reader_.emplace(filepath_);
    produced_ = false;
}

bool MetadataScan::Next(ExecBatch& out) {
    COLUMNAR_ASSERT(reader_.has_value(), "MetadataScan is not opened");

    if (produced_) {
        return false;
    }

    Schema schema;
    for (const auto& column : columns_) {
        schema.AddColumn(column.outName, column.outType);
    }

    RowGroupBuilder builder(std::move(schema));
    for (size_t i = 0; i < columns_.size(); ++i) {
        if (columns_[i].outType != Types::LogicalType::INT64) {
            throw std::runtime_error("Not implemented yet");
        }
        builder.Append(i, ReadMetaField(columns_[i].field));
    }

    out.Reset();
    out.rowGroup.emplace(builder.Finish());
    out.rowCount = 1;

    produced_ = true;
    return true;
}

void MetadataScan::Close() noexcept {
    reader_.reset();
    produced_ = false;
}

int64_t MetadataScan::ReadMetaField(MetadataField field) const {
    switch (field) {
        case MetadataField::TotalRowCount:
            return static_cast<int64_t>(reader_->GetTotalRowCount());
        case MetadataField::RowGroupCount:
            return static_cast<int64_t>(reader_->GetRowGroupCount());
        case MetadataField::ColCount:
            return static_cast<int64_t>(reader_->GetSchema().GetColumnCount());
        case MetadataField::ColMin:
        case MetadataField::ColMax:
        case MetadataField::ColNullCount:
            throw std::runtime_error("Not implemented yet");
    }
    throw std::runtime_error("Not implemented yet");
}

}  // namespace Columnar::Exec
