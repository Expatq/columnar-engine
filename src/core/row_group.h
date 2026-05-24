#pragma once

#include <core/column.h>
#include <core/schema.h>

#include <absl/container/inlined_vector.h>

#include <optional>
#include <vector>

namespace Columnar {

class RowGroup {
    using const_iterator = absl::InlinedVector<Column, 8>::const_iterator;

public:
    explicit RowGroup(Schema schema, std::vector<Column> columns);

    RowGroup(RowGroup&&) noexcept;
    RowGroup& operator=(RowGroup&&) noexcept;

    size_t GetColumnCount() const;
    size_t GetRowCount() const;

    const Schema& GetSchema() const;

    const Column& GetColumn(size_t index) const;
    Column& GetColumn(size_t index);

    const Column* FindColumn(const std::string& name) const;
    Column* FindColumn(const std::string& name);

    const_iterator begin() const {
        return columns_.begin();
    }

    const_iterator end() const {
        return columns_.end();
    }

private:
    RowGroup(const RowGroup&) = delete;
    RowGroup& operator=(const RowGroup&) = delete;

    void Validate() const;

private:
    Schema schema_;
    absl::InlinedVector<Column, 8> columns_;
    size_t rowCount_ = 0;
};

};  // namespace Columnar