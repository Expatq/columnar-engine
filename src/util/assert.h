#pragma once

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace Columnar {

[[noreturn]] inline void AssertFail(const char* cond, std::string_view msg,
                                    const char* file, int line) {
    std::fprintf(stderr,
                 "COLUMNAR_ASSERT failed: %s\n  msg: %.*s\n  at %s:%d\n", cond,
                 static_cast<int>(msg.size()), msg.data(), file, line);
    std::abort();
}

}  // namespace Columnar

#define COLUMNAR_ASSERT(cond, msg)                                    \
    do {                                                              \
        if (!(cond)) [[unlikely]] {                                   \
            std::fprintf(stderr, "Function caused termination: %s",   \
                         __PRETTY_FUNCTION__);                        \
            ::Columnar::AssertFail(#cond, (msg), __FILE__, __LINE__); \
        }                                                             \
    } while (0)
