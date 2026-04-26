#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace Columnar::Util {

class Timer {
public:
    using Clock = std::chrono::steady_clock;

    Timer();

    void Reset();

    double ElapsedSeconds() const;
    int64_t ElapsedMilliseconds() const;

private:
    Clock::time_point startedAt_;
};

std::string FormatSeconds(double seconds);
std::string FormatRowsPerSecond(uint64_t rows, double seconds);

}  // namespace Columnar::Util
