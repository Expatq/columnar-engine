#pragma once

#include <cstdint>

namespace Columnar::Calendar {

// ─── Time unit constants ───────────────────────────────────────────────────────

inline constexpr int64_t kSecondsPerMinute = 60;
inline constexpr int64_t kSecondsPerHour   = 3'600;
inline constexpr int64_t kSecondsPerDay    = 86'400;

// ─── Howard Hinnant's civil calendar algorithm: Gregorian → epoch days ─────────
// https://howardhinnant.github.io/date_algorithms.html
// No floating point, no alloc, no locale, no mktime. ~10 ns.

inline constexpr int kGregorianCycleYears = 400;     // years in one Gregorian cycle
inline constexpr int kDaysPerCycle        = 146'097; // days in a 400-year cycle
inline constexpr int kCivilToUnixDays     = 719'468; // offset: 0000-03-01 → 1970-01-01
// Coefficients for accumulated days per month (months counted from March):
//   floor((153*m + 2) / 5) reproduces day counts exactly for m in [0,11].
inline constexpr int kMonthDayNum = 153;
inline constexpr int kMonthDayDen = 5;

constexpr int32_t GregorianToEpochDays(int y, int m, int d) noexcept {
    y -= (m <= 2);
    const int era      = (y >= 0 ? y : y - (kGregorianCycleYears - 1)) / kGregorianCycleYears;
    const unsigned yoe = static_cast<unsigned>(y - era * kGregorianCycleYears);
    const unsigned doy = (kMonthDayNum * static_cast<unsigned>(m + (m > 2 ? -3 : 9)) + 2)
                         / kMonthDayDen
                         + static_cast<unsigned>(d) - 1;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * kDaysPerCycle + static_cast<int>(doe) - kCivilToUnixDays;
}

}  // namespace Columnar::Calendar
