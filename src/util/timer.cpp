#include <util/timer.h>

#include <iomanip>
#include <sstream>

namespace Columnar::Util {

Timer::Timer()
    : startedAt_(Clock::now()) {
}

void Timer::Reset() {
    startedAt_ = Clock::now();
}

double Timer::ElapsedSeconds() const {
    const auto elapsed = Clock::now() - startedAt_;
    return std::chrono::duration<double>(elapsed).count();
}

int64_t Timer::ElapsedMilliseconds() const {
    const auto elapsed = Clock::now() - startedAt_;
    return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
        .count();
}

std::string FormatSeconds(double seconds) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << seconds << "s";
    return out.str();
}

std::string FormatRowsPerSecond(uint64_t rows, double seconds) {
    std::ostringstream out;
    if (seconds <= 0.0) {
        out << "inf rows/s";
        return out.str();
    }

    const double rowsPerSecond = static_cast<double>(rows) / seconds;
    out << std::fixed << std::setprecision(2) << rowsPerSecond << " rows/s";
    return out.str();
}

}  // namespace Columnar::Util
