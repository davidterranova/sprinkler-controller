#pragma once
//
//  irrigation_core -- pure C++17. ZERO ESPHome includes, by design and by CI gate.
//
//  Everything in this directory must compile and run on a host with nothing but
//  a standard library, so the scheduler and the state machine can be tested
//  against a table of awkward dates instead of against a garden. The gate that
//  enforces it is `make purity-gate`.
//
//  It lives under components/ rather than lib/ (as docs/17-repository-layout.md
//  proposed) for one practical reason: ESPHome only copies source files that
//  belong to a component package, so a sibling lib/ directory would never reach
//  the firmware build. Making it a config-less ESPHome component keeps ONE copy
//  of the source shared by the host tests and the device build.
//
#include <cstdint>

namespace irrigation_core {

// Compile-time ceiling, not the configured count. The bench runs 4 zones; the
// design targets 8. Keeping the array bound at 8 means growing is a YAML edit
// and a schema-version bump, not a struct-layout migration.
inline constexpr uint8_t MAX_ZONES = 8;

// Multiple programs per day is FR-5.7 (upgraded by D5 -- heavy clay wants
// several short passes rather than one long one).
inline constexpr uint8_t MAX_PROGRAMS = 4;

// Seconds since 1970-01-01T00:00:00Z. Always UTC -- see docs/07-firmware.md:
// storing local time turns a DST transition into a *write*, and every reboot
// near the transition then has to work out whether that write already happened.
using Epoch = int64_t;

// Civil time in the configured zone, already resolved. The resolver never does
// timezone arithmetic itself; it is handed a LocalTime and reasons about it.
struct LocalTime {
  int16_t year{0};
  uint8_t month{0};   // 1-12
  uint8_t day{0};     // 1-31
  uint8_t hour{0};
  uint8_t minute{0};
  uint8_t second{0};
  uint8_t weekday{0};  // 0 = Sunday .. 6 = Saturday

  // Days since 1970-01-01 counted in *local* civil terms. This is the field the
  // resolver actually keys on: it is the "local_date" of the idempotency key
  // (zone, program, local_date), and it advances exactly once per civil day
  // even across a DST transition, which an epoch/86400 division does not.
  int32_t day_number{0};

  bool valid{false};

  uint16_t minute_of_day() const { return static_cast<uint16_t>(hour) * 60 + minute; }
};

// Days since 1970-01-01 from a proleptic Gregorian y/m/d. Howard Hinnant's
// days_from_civil, which is exact for the whole range we care about and has no
// libc or timezone dependency at all.
constexpr int32_t days_from_civil(int32_t y, uint32_t m, uint32_t d) {
  y -= m <= 2;
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const uint32_t yoe = static_cast<uint32_t>(y - era * 400);              // [0, 399]
  const uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;    // [0, 365]
  const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;             // [0, 146096]
  return era * 146097 + static_cast<int32_t>(doe) - 719468;
}

}  // namespace irrigation_core
