#pragma once

#include <io/binary/file_reader.h>

#include <core/row_group.h>
#include <core/row_group_meta.h>
#include <core/schema.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Columnar::IO {

class FormatReader {
public:
    explicit FormatReader(const std::string& filename);
    ~FormatReader() = default;

    FormatReader(const FormatReader&) = delete;
    FormatReader& operator=(const FormatReader&) = delete;

    FormatReader(FormatReader&&) noexcept = default;
    FormatReader& operator=(FormatReader&&) noexcept = default;

    std::optional<RowGroup> ReadRowGroup();
    std::optional<RowGroup> ReadRowGroup(const std::vector<std::string>& colNames);

    bool HasMore() const;

    const Schema& GetSchema() const;
    size_t GetRowGroupCount() const;
    const RowGroupMeta& GetRowGroupMeta(size_t index) const;
    uint64_t GetTotalRowCount() const;
    uint32_t GetRowGroupRows(size_t index) const;

private:
    FileReader file_;
    size_t pos_ = 0;

    uint32_t columnCount_ = 0;
    uint64_t totalRowCount_ = 0;
    uint64_t footerOffset_ = 0;

    Schema schema_;
    std::vector<RowGroupMeta> rowGroupMetas_;
    size_t curRowGroupIdx_ = 0;

    template <typename T>
    T ReadField() {
        T v;
        file_.Read(pos_, &v, sizeof(T));
        pos_ += sizeof(T);
        return v;
    }

    void ReadBytes(void* dst, size_t n) {
        file_.Read(pos_, dst, n);
        pos_ += n;
    }

    std::string ReadString();

    void ValidateMagic();
    void ReadHeader();
    void ReadSchema();
    void ReadFooter();

    std::vector<size_t> ResolveColumnNames(
        const std::vector<std::string>& colNames) const;

    RowGroup ReadAllColumns(const RowGroupMeta& meta);
    RowGroup ReadSelectedColumns(const RowGroupMeta& meta,
                                 const std::vector<size_t>& colIndices);

    Column ReadColumn(Types::PhysicalType physical, size_t rowCount);
};

}  // namespace Columnar::IO
