#pragma once

#include <core/row_group.h>
#include <core/schema.h>
#include <io/binary_io.h>

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

    FormatReader(FormatReader&&) = default;
    FormatReader& operator=(FormatReader&&) = default;

    void Open();

    std::optional<Batch> ReadBatch();
    bool HasMore() const;
    RowGroup ReadRowGroup(size_t index);

    const Schema& GetSchema() const;
    size_t GetRowGroupCount() const;
    const RowGroupMeta& GetRowGroupMeta(size_t index) const;
    uint64_t GetTotalRowCount() const;

private:
    BinaryReader reader_;
    bool opened_ = false;

    uint32_t columnCount_ = 0;
    uint64_t totalRowCount_ = 0;
    uint64_t footerOffset_ = 0;

    Schema schema_;
    std::vector<RowGroupMeta> rowGroupMetas_;
    size_t currentRowGroupIndex_ = 0;

    void ValidateMagic();
    void ReadHeader();
    void ReadSchema();
    void ReadFooter();
    Column ReadColumn(const std::string& name, Types::DataType type,
                      size_t rowCount);
};

}  // namespace Columnar::IO
