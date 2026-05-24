#include <util/str.h>
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace str {

std::string strip(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string_view strip(std::string_view sv) {
    size_t start = sv.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return {};
    }
    size_t end = sv.find_last_not_of(" \t\r\n");
    return sv.substr(start, end - start + 1);
}

std::string tolower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return str;
}

}  // namespace str