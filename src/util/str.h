#pragma once

#include <string>
#include <string_view>

namespace str {

std::string strip(const std::string& str);

std::string_view strip(std::string_view sv);

std::string tolower(std::string str);

}  // namespace str