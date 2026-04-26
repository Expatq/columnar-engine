#pragma once

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace Columnar {

[[noreturn]] inline void AssertFail(const char* condition,
                                    std::string_view message,
                                    const char* file,
                                    int line,
                                    const char* function) noexcept {
    std::fprintf(stderr,
                 "\n"
                 "COLUMNAR_ASSERT failed\n"
                 "  condition : %s\n"
                 "  message   : %.*s\n"
                 "  location  : %s:%d\n"
                 "  function  : %s\n"
                 "\n",
                 condition,
                 static_cast<int>(message.size()),
                 message.data(),
                 file,
                 line,
                 function);
    std::fflush(stderr);
    std::abort();
}

}  // namespace Columnar

#define COLUMNAR_ASSERT(cond, msg)                       \
    do {                                                 \
        if (!(cond)) [[unlikely]] {                      \
            ::Columnar::AssertFail(#cond,                \
                                   (msg),                \
                                   __builtin_FILE(),     \
                                   __builtin_LINE(),     \
                                   __PRETTY_FUNCTION__); \
        }                                                \
    } while (0)
