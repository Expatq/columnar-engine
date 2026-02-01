#pragma once

#include <core/column.h>
#include <core/schema.h>
#include <vector>

namespace Columnar {

constexpr size_t kBatchSize = 2048;

class Batch {
public:
    // ctors
    Batch() = default;

    explicit Batch(std::vector<Column> columns);

    Batch(Schema schema, std::vector<Column> columns);

    static Batch CreateEmpty(const Schema& schema);

    // batch is move only
    Batch(const Batch&) = delete;
    Batch& operator=(const Batch&) = delete;

    Batch(Batch&&) noexcept = default;
    Batch& operator=(Batch&&) noexcept = default;

    // Get meta

    size_t GetColumnCount() const;
    size_t GetRowCount() const;
    bool IsEmpty() const;
    bool IsFull() const;

    const Schema& GetSchema() const;

    // Columns access

    const Column& GetColumn(size_t index) const;
    Column& GetMutableColumn(size_t index);

    const Column* FindColumn(const std::string& name) const;
    Column* FindMutableColumn(const std::string& name);

    // iterators

    using iterator = std::vector<Column>::iterator;
    using const_iterator = std::vector<Column>::const_iterator;

    iterator begin() { return columns_.begin(); }

    iterator end() { return columns_.end(); }

    const_iterator begin() const { return columns_.begin(); }

    const_iterator end() const { return columns_.end(); }

    // modification

    bool AppendRow(std::vector<std::string>&& values);

    void Reserve(size_t capacity);

    void Clear();

    // checks that all columns have same len
    bool IsValid() const;

private:
    Schema schema_;
    std::vector<Column> columns_;
    size_t rowCount_ = 0;

    void ValidateColumns() const;
    void UpdateRowCount();
};

};  // namespace Columnar