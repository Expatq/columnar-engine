#include <core/column.h>
#include <core/types.h>
#include <parser/value_parser.h>

#include <stdexcept>
#include <variant>

namespace Columnar {

Column::Column(std::string name, Types::DataType type)
    : name_(std::move(name)),
      type_(type),
      data_(Types::CreateEmptyColumnData(type)) {}

Column::Column(std::string name, Types::DataType type,
               Types::AnyColumnData data)
    : name_(std::move(name)),
      type_(type),
      data_(std::move(data)) {}

Column Column::CreateInt16(const std::string& name, std::vector<int16_t> data) {
    return Column(name, Types::DataType::INT16, std::move(data));
}

Column Column::CreateInt32(const std::string& name, std::vector<int32_t> data) {
    return Column(name, Types::DataType::INT32, std::move(data));
}

Column Column::CreateInt64(const std::string& name, std::vector<int64_t> data) {
    return Column(name, Types::DataType::INT64, std::move(data));
}

Column Column::CreateBool(const std::string& name, std::vector<bool> data) {
    return Column(name, Types::DataType::BOOL, std::move(data));
}

Column Column::CreateString(const std::string& name,
                            std::vector<std::string> data) {
    return Column(name, Types::DataType::STRING, std::move(data));
}

const std::string& Column::GetName() const {
    return name_;
}

Types::DataType Column::GetType() const {
    return type_;
}

size_t Column::GetRowCount() const {
    return std::visit(Types::GetSizeVisitor{}, data_);
}

bool Column::IsEmpty() const {
    return GetRowCount() == 0;
}

const Types::AnyColumnData& Column::GetData() const {
    return data_;
}

Types::AnyColumnData& Column::GetMutableData() {
    return data_;
}

std::string Column::GetValueAsString(size_t row) const {
    if (row >= GetRowCount()) {
        throw std::out_of_range("Row index out of range: " +
                                std::to_string(row));
    }

    return std::visit(
        Types::overloaded{
            [row](const std::vector<int16_t>& vec) {
                return std::to_string(vec[row]);
            },
            [row](const std::vector<int32_t>& vec) {
                return std::to_string(vec[row]);
            },
            [row](const std::vector<int64_t>& vec) {
                return std::to_string(vec[row]);
            },
            [row](const std::vector<bool>& vec) {
                return vec[row] ? std::string{"true"} : std::string{"false"};
            },
            [row](const std::vector<std::string>& vec) { return vec[row]; }},
        data_);
}

void Column::AppendFromString(std::string&& value) {
    auto parsed = Parser::ParseValue(std::move(value), type_);

    std::visit(Types::overloaded{
                   [&parsed](std::vector<int16_t>& v) {
                       v.push_back(std::get<int16_t>(parsed));
                   },
                   [&parsed](std::vector<int32_t>& v) {
                       v.push_back(std::get<int32_t>(parsed));
                   },
                   [&parsed](std::vector<int64_t>& v) {
                       v.push_back(std::get<int64_t>(parsed));
                   },
                   [&parsed](std::vector<bool>& v) {
                       v.push_back(std::get<bool>(parsed));
                   },
                   [&parsed](std::vector<std::string>& v) {
                       v.push_back(std::get<std::string>(parsed));
                   },
               },
               data_);
}

void Column::Reserve(size_t capacity) {
    Types::ReserveVisitor visiror{capacity};
    std::visit(visiror, data_);
}

void Column::Clear() {
    std::visit(Types::ClearVisitor{}, data_);
}

}  // namespace Columnar
