#include "schedule.h"

namespace irrigation_core {

const char *ValidationResult::message() const {
  switch (this->error) {
    case ValidationError::NONE:
      return "ok";
    case ValidationError::BAD_SCHEMA:
      return "schema version not understood by this firmware";
    case ValidationError::BAD_ZONE_COUNT:
      return "zone count out of range";
    case ValidationError::START_OUT_OF_RANGE:
      return "start time is not a minute of the day";
    case ValidationError::DST_START_WINDOW:
      return "start time falls in 01:30-03:30, which DST deletes or repeats";
    case ValidationError::DURATION_TOO_LONG:
      return "zone duration exceeds the 120 min schedule cap";
    case ValidationError::SEASONAL_OUT_OF_RANGE:
      return "seasonal adjustment outside 0-200 %";
    case ValidationError::REPEATS_OUT_OF_RANGE:
      return "cycle-and-soak passes outside 1-6";
    case ValidationError::NO_DAY_SELECTED:
      return "program is enabled but no weekday is selected";
  }
  return "unknown";
}

ValidationResult validate(const Schedule &s) {
  ValidationResult r;

  if (s.schema_version != SCHEMA_VERSION) {
    r.error = ValidationError::BAD_SCHEMA;
    return r;
  }
  if (s.zone_count == 0 || s.zone_count > MAX_ZONES) {
    r.error = ValidationError::BAD_ZONE_COUNT;
    return r;
  }
  if (s.seasonal_pct > MAX_SEASONAL_PCT) {
    r.error = ValidationError::SEASONAL_OUT_OF_RANGE;
    return r;
  }

  for (uint8_t p = 0; p < MAX_PROGRAMS; p++) {
    const Program &prog = s.programs[p];
    r.program = p;

    // A disabled program is not required to be coherent -- it is a draft.
    if (!prog.enabled)
      continue;

    if (prog.start_minute >= 24 * 60) {
      r.error = ValidationError::START_OUT_OF_RANGE;
      return r;
    }
    if (prog.start_minute >= DST_FORBIDDEN_START_MIN && prog.start_minute <= DST_FORBIDDEN_START_MAX) {
      r.error = ValidationError::DST_START_WINDOW;
      return r;
    }
    if (prog.repeats < 1 || prog.repeats > MAX_REPEATS) {
      r.error = ValidationError::REPEATS_OUT_OF_RANGE;
      return r;
    }
    if (prog.days.weekday_mask == 0) {
      r.error = ValidationError::NO_DAY_SELECTED;
      return r;
    }
    for (uint8_t z = 0; z < s.zone_count; z++) {
      if (prog.zone_minutes[z] > MAX_ZONE_MINUTES) {
        r.error = ValidationError::DURATION_TOO_LONG;
        r.zone = z;
        return r;
      }
    }
  }

  r.program = 0;
  return r;
}

Schedule default_schedule(uint8_t zone_count) {
  Schedule s;
  s.schema_version = SCHEMA_VERSION;
  s.zone_count = zone_count == 0 || zone_count > MAX_ZONES ? MAX_ZONES : zone_count;
  s.seasonal_pct = 100;
  for (uint8_t z = 0; z < MAX_ZONES; z++) {
    s.zone_enabled[z] = z < s.zone_count;
    // Every zone at the SAME priority, not one priority each. Unique
    // priorities would order the plan deterministically -- and would also
    // switch rotation off entirely, because rotation happens *within* an
    // equal-priority group. FR-4.5 exists so the same zone is not always last,
    // and a default that quietly defeats it is worse than no default. Equal
    // priority means the plan rotates from day one; an operator who wants a
    // zone pinned first lowers its number.
    s.zone_priority[z] = 0;
  }
  for (uint8_t p = 0; p < MAX_PROGRAMS; p++) {
    Program &prog = s.programs[p];
    prog.enabled = false;
    // 05:00 -- safe on both DST transitions and agronomically better anyway
    // (docs/07-firmware.md). Only a default; the program is off.
    prog.start_minute = 5 * 60;
    prog.days.weekday_mask = 0x7F;
    prog.days.interval_days = 0;
    prog.repeats = 1;
    for (uint8_t z = 0; z < MAX_ZONES; z++)
      prog.zone_minutes[z] = 0;
  }
  return s;
}

uint16_t effective_seconds(uint16_t nominal_minutes, uint16_t seasonal_pct, uint8_t repeats) {
  if (nominal_minutes == 0)
    return 0;
  if (repeats < 1)
    repeats = 1;
  if (seasonal_pct > MAX_SEASONAL_PCT)
    seasonal_pct = MAX_SEASONAL_PCT;

  // Widened before multiplying: 120 min * 200 % * 60 s overflows uint16_t.
  uint32_t seconds = static_cast<uint32_t>(nominal_minutes) * 60u;
  seconds = seconds * seasonal_pct / 100u;
  seconds /= repeats;

  // A pass short enough to be hydraulically pointless is worse than no pass:
  // the diaphragm barely lifts, the lateral does not pressurise, and the flow
  // check would read it as a fault. Round anything nonzero up to 30 s.
  if (seconds > 0 && seconds < 30)
    seconds = 30;

  const uint32_t cap = static_cast<uint32_t>(MAX_ZONE_MINUTES) * 60u;
  if (seconds > cap)
    seconds = cap;
  return static_cast<uint16_t>(seconds);
}

}  // namespace irrigation_core
