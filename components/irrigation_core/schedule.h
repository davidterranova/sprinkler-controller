#pragma once
//
//  The schedule model, and the validation that stands between an operator's
//  typo and the garden.
//
#include "types.h"

namespace irrigation_core {

// Bumped whenever the persisted layout changes. A firmware that reads a record
// it does not understand must REFUSE it and alarm, never guess (FR-5.8).
inline constexpr uint16_t SCHEMA_VERSION = 1;

// A scheduled zone run is capped at 2 h. This is a *schedule* limit; the
// independent safety limit lives in the guard (see guard.h MAX_RUN_SECONDS) and
// in the hardware max-runtime backstop, and neither trusts this number.
inline constexpr uint16_t MAX_ZONE_MINUTES = 120;

// FR-10.3: a manual run's hard maximum, enforced on the device.
inline constexpr uint16_t MAX_MANUAL_MINUTES = 60;

// FR-5.6: a global seasonal percentage, clamped so 300 % cannot be typed in.
inline constexpr uint16_t MAX_SEASONAL_PCT = 200;

// FR-5.10 cycle-and-soak: N passes over the zone list at duration/N each.
inline constexpr uint8_t MAX_REPEATS = 6;

// FR-5.9. Europe/Paris deletes 02:00-03:00 in spring and repeats it in autumn.
// A start time inside that band is not a policy we get to define -- it is a
// time that either does not exist or exists twice. Rejected at validation.
inline constexpr uint16_t DST_FORBIDDEN_START_MIN = 1 * 60 + 30;  // 01:30
inline constexpr uint16_t DST_FORBIDDEN_START_MAX = 3 * 60 + 30;  // 03:30

// FR-5.1: independent, composable day filters.
struct DayPredicate {
  uint8_t weekday_mask{0};   // bit 0 = Sunday .. bit 6 = Saturday; 0 = never
  uint8_t interval_days{0};  // 0 = filter disabled; else "every N days"
};

struct Program {
  bool enabled{false};
  uint16_t start_minute{0};  // minutes since local midnight
  DayPredicate days{};
  uint8_t repeats{1};                     // >= 1
  uint16_t zone_minutes[MAX_ZONES]{};     // 0 => this zone is not in this program
};

struct Schedule {
  uint16_t schema_version{SCHEMA_VERSION};
  uint8_t zone_count{0};
  uint16_t seasonal_pct{100};
  bool zone_enabled[MAX_ZONES]{};
  uint8_t zone_priority[MAX_ZONES]{};  // lower number runs earlier
  Program programs[MAX_PROGRAMS]{};
};

enum class ValidationError : uint8_t {
  NONE = 0,
  BAD_SCHEMA,
  BAD_ZONE_COUNT,
  START_OUT_OF_RANGE,
  DST_START_WINDOW,
  DURATION_TOO_LONG,
  SEASONAL_OUT_OF_RANGE,
  REPEATS_OUT_OF_RANGE,
  NO_DAY_SELECTED,
};

struct ValidationResult {
  ValidationError error{ValidationError::NONE};
  uint8_t program{0};  // which program failed, where meaningful
  uint8_t zone{0};

  bool ok() const { return error == ValidationError::NONE; }
  const char *message() const;
};

// Pure. Never mutates; the caller decides whether to commit.
ValidationResult validate(const Schedule &s);

// A schedule that is safe to boot with when nothing has ever been committed
// (FR-2.4). Deliberately conservative: every program disabled. A device that
// has never been configured must not invent a watering plan.
Schedule default_schedule(uint8_t zone_count);

// Applies the seasonal percentage and cycle-and-soak division to a nominal
// per-zone duration, and clamps. Returns seconds. Exposed (rather than hidden
// in the resolver) because it is the arithmetic most worth testing directly.
uint16_t effective_seconds(uint16_t nominal_minutes, uint16_t seasonal_pct, uint8_t repeats);

}  // namespace irrigation_core
