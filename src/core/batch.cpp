#include <core/batch.h>
#include <core/column.h>
#include <core/schema.h>

#include <stdexcept>
#include <string>

namespace Columnar {

Batch::Batch(std::vector<Column> columns)
    : columns_(std::move(columns)) {
    for (const auto& col : columns_) {
        schema_.AddColumn(col.GetName(), col.GetType());
    }

    ValidateColumns();
    UpdateRowCount();
}

Batch::Batch(Schema schema, std::vector<Column> columns)
    : schema_(std::move(schema)),
      columns_(std::move(columns)) {
    ValidateColumns();
    UpdateRowCount();
}

Batch Batch::CreateEmpty(const Schema& schema) {
    std::vector<Column> columns;
    columns.reserve(schema.GetColumnCount());

    for (const auto& colSchema : schema) {
        columns.emplace_back(colSchema.name, colSchema.type);
    }

    return Batch(schema, std::move(columns));
}

size_t Batch::GetColumnCount() const {
    return columns_.size();
}

size_t Batch::GetRowCount() const {
    return rowCount_;
}

bool Batch::IsEmpty() const {
    return rowCount_ == 0;
}

bool Batch::IsFull() const {
    return rowCount_ >= kBatchSize;
}

const Schema& Batch::GetSchema() const {
    return schema_;
}

const Column& Batch::GetColumn(size_t index) const {
    if (index >= columns_.size()) {
        throw std::out_of_range("Column index out of range: " +
                                std::to_string(index));
    }
    return columns_[index];
}

Column& Batch::GetMutableColumn(size_t index) {
    if (index >= columns_.size()) {
        throw std::out_of_range("Column index out of range: " +
                                std::to_string(index));
    }
    return columns_[index];
}

const Column* Batch::FindColumn(const std::string& name) const {
    auto idx = schema_.FindColumn(name);
    if (!idx) {
        return nullptr;
    }

    return &columns_[*idx];
}

Column* Batch::FindMutableColumn(const std::string& name) {
    auto idx = schema_.FindColumn(name);
    if (!idx) {
        return nullptr;
    }

    return &columns_[*idx];
}

bool Batch::AppendRow(std::vector<std::string>&& values) {
    if (rowCount_ >= kBatchSize) {
        return false;
    }

    if (values.size() != columns_.size()) {
        throw std::invalid_argument("Value count mismatch: expected " +
                                    std::to_string(columns_.size()) + ", got " +
                                    std::to_string(values.size()));
    }

    for (size_t i = 0; i < columns_.size(); ++i) {
        columns_[i].AppendFromString(std::move(values[i]));
    }

    ++rowCount_;
    return true;
}

void Batch::Reserve(size_t capacity) {
    for (auto& col : columns_) {
        col.Reserve(capacity);
    }
}

void Batch::Clear() {
    for (auto& col : columns_) {
        col.Clear();
    }
    rowCount_ = 0;
}

bool Batch::IsValid() const {
    if (columns_.empty()) {
        return true;
    }

    size_t expectedRows = columns_[0].GetRowCount();
    for (const auto& col : columns_) {
        if (col.GetRowCount() != expectedRows) {
            return false;
        }
    }

    return true;
}

void Batch::ValidateColumns() const {
    if (columns_.empty()) {
        return;
    }

    size_t expectedRows = columns_[0].GetRowCount();
    for (size_t i = 1; i < columns_.size(); ++i) {
        if (columns_[i].GetRowCount() != expectedRows) {
            throw std::invalid_argument(
                "Column size mismatch: column 0 has " +
                std::to_string(expectedRows) + " rows, column " +
                std::to_string(i) + " has " +
                std::to_string(columns_[i].GetRowCount()) + " rows");
        }
    }
}

void Batch::UpdateRowCount() {
    if (columns_.empty()) {
        rowCount_ = 0;
    } else {
        rowCount_ = columns_[0].GetRowCount();
    }
}

}  // namespace Columnar