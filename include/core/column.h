#pragma once

#include <core/types.h>
#include <string>
#include <vector>

namespace Columnar {

class Column {
public:
    // ctors
    Column() = default;

    Column(std::string name, Types::DataType type);

    Column(std::string name, Types::DataType, Types::AnyColumnData);

    // Factories
    static Column CreateInt16(const std::string& name,
                              std::vector<int16_t> data);
    static Column CreateInt32(const std::string& name,
                              std::vector<int32_t> data);
    static Column CreateInt64(const std::string& name,
                              std::vector<int64_t> data);
    static Column CreateBool(const std::string& name, std::vector<bool> data);
    static Column CreateString(const std::string& name,
                               std::vector<std::string> data);

    // Get meta
    const std::string& GetName() const;
    Types::DataType GetType() const;
    size_t GetRowCount() const;
    bool IsEmpty() const;

    // Data access
    const Types::AnyColumnData& GetData() const;
    Types::AnyColumnData& GetMutableData();

    std::string GetValueAsString(size_t row) const;

    template <typename T>
    const std::vector<T>& GetTypedData() const {
        return std::get<std::vector<T>>(data_);
    }

    template <typename T>
    std::vector<T>& GetMutuableTypedData() {
        return std::get<std::vector<T>>(data_);
    }

    // Modification

    void AppendFromString(std::string&& value);

    void Reserve(size_t capacity);

    void Clear();

private:
    std::string name_;
    Types::DataType type_;  // Maybe set default to str
    Types::AnyColumnData data_;
};

}  // namespace Columnar